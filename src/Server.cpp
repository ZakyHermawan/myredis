#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <future>
#include <memory>
#include <numeric>
#include <shared_mutex>
#include <unordered_map>

#include "database.hpp"
#include "eventloop.hpp"
#include "parser.hpp"

#define MAX_BUFF_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

std::shared_ptr<Database> db;
std::shared_mutex db_mutex;

class Context {
public:
  std::unordered_map<std::string, std::string> m_info;
  Context() {
    m_info["role"] = "master";
  }
};

std::shared_ptr<Context> ctx;
std::shared_mutex ctx_mutex;

void eventHandler(int client_fd) {
  while(true) {
    char* tmp_buffer = (char*)malloc(sizeof(char) * MAX_BUFF_SIZE);
    while (recv(client_fd, tmp_buffer, MAX_BUFF_SIZE, 0)) {
      Buffer buff(tmp_buffer);

      std::string response;
      char first_byte = buff.getNextChar();
      if(first_byte == '*') {
        // arrays
        size_t num_elements = buff.getNextChar() - '0';        
        buff.remove_clrf();

        // currently only accept array of bulk strings
        std::vector<std::string> arr;

        for(size_t i=0; i<num_elements; ++i) {
          char data_type = buff.getNextChar();
          if(data_type == '$') {
            // bulk strings

            std::vector<char> numchar;
            char lookahead_char;
            do {
              numchar.push_back(buff.getNextChar());
              lookahead_char = buff.lookNextChar();
            } while(lookahead_char != '\r');
            auto base10_digits = [](char a, char b) {
              return 10 * a + (b - '0');
            };
            size_t num_bytes = std::accumulate(numchar.begin(), numchar.end(), 0, base10_digits);
            buff.remove_clrf();

            std::string tmp;

            for(size_t j=0; j<num_bytes; ++j) {
              tmp.push_back(buff.getNextChar());
            }
            tmp[num_bytes] = 0;
            arr.push_back(tmp);
            
            buff.remove_clrf();
          }
        }
        // convert to lower
        std::transform(arr[0].begin(), arr[0].end(), arr[0].begin(),
          [](unsigned char c){ return std::tolower(c); });
        if(arr[0] == "ping") {
        // handle ping
        response = "+PONG\r\n";
        }
        else if(arr[0] == "echo") {
          response = compose_bulk_string(arr[1]);
        }
        else if(arr[0] == "set") {
          // do set operation
          std::unique_lock<std::shared_mutex> write_lock(db_mutex);
          std::string key = arr[1], value = arr[2];

          db->set_value(key, value);
          if(num_elements == 5) {
            // set expiry
            // ex: redis-cli set foo bar px 100
            std::transform(arr[3].begin(), arr[3].end(), arr[3].begin(),
            [](unsigned char c){ return std::tolower(c); });

            if(arr[3] == "px") {
              auto duration = std::chrono::milliseconds(std::stoi(arr[4]));
              db->set_expiry(key, duration);
            }
          }
          response = "+OK\r\n";
        }
        else if(arr[0] == "get") {
          // do get operation
          std::shared_lock<std::shared_mutex> read_lock(db_mutex);
          std::string key = arr[1];
          std::string val = db->get_value(key);

          if(val == "") {
            response = "$-1\r\n";
          }
          else {
            std::string key = arr[1];
            std::chrono::system_clock::time_point insert_timestamp = db->get_timestamp(key);
            std::chrono::system_clock::time_point current_timestamp = std::chrono::system_clock::now();

            ms_type time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(current_timestamp - insert_timestamp);
            ms_type expiry = db->get_expiry(key);
            if(expiry == std::chrono::milliseconds(0) or time_diff < expiry) {
              response = compose_bulk_string(val);
            }
            else {
              response = "$-1\r\n";
            }
          }
        }
        else if(arr[0] == "info") {
          std::shared_lock<std::shared_mutex> read_lock(ctx_mutex);
          std::string payload = "role:" + ctx->m_info["role"] + "\r\n";
          if(arr[1] == "replication") {
            payload += "master_replid:" + ctx->m_info["master_replid"] + "\r\n";
            payload += "master_repl_offset:" + ctx->m_info["master_repl_offset"] + "\r\n";
          }
          response = compose_bulk_string(payload);
        }
        send(client_fd, response.c_str(), response.size(), 0);
      }
    }
  }
}


void replica_entry_point() {
  // begin handshake
  int replicate_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  struct sockaddr_in master_addr;
  master_addr.sin_family = AF_INET;
  master_addr.sin_port = htons(std::stoi(ctx->m_info["host_port"]));

  if(inet_pton(AF_INET, ctx->m_info["host_name"].c_str(), &master_addr.sin_addr) <= 0) {
    std::cerr << "Invalid address or address is not supported\n";
    return;
  }
  if(connect(replicate_fd, (struct sockaddr*)&master_addr, sizeof(master_addr)) < 0) {
    std::cerr << "Connection failed\n";
    return;
  }

  std::string ping = "*1\r\n$4\r\nping\r\n";
  send(replicate_fd, ping.c_str(), ping.length(), 0);
  char buff[1000];
  recv(replicate_fd, buff, sizeof(buff), 0);
  std::string message_1 = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n" + std::to_string(6380) +"\r\n";
  send(replicate_fd, message_1.c_str(), message_1.length(), 0);
  std::string message_2 = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
  send(replicate_fd, message_2.c_str(), message_2.length(), 0);
}

int main(int argc, char **argv) {
  db = std::make_shared<Database>();
  ctx = std::make_shared<Context>();

  ctx->m_info["master_replid"] = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
  ctx->m_info["master_repl_offset"] = "0";

  int port = 6379;

  if(argc > 1) {
    for(int i=0; i<argc; ++i) {
      if(strcmp(argv[i], "--port") == 0) {
        sscanf(argv[i+1], "%d", &port);
        ctx->m_info["listen_port"] = std::string(argv[i+1]);
      }
      else if(strcmp(argv[i], "--replicaof") == 0) {
        ctx->m_info["role"] = "slave";
        ctx->m_info["host_name"] = std::string(argv[i+1]);
        ctx->m_info["host_port"] = std::string(argv[i+2]);
        if(ctx->m_info["host_name"] == "localhost") {
          ctx->m_info["host_name"] = "127.0.0.1";
        }
        replica_entry_point();
      }
    }
  }

  std::cout << "Logs from your program will appear here!\n";
  int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      std::cout << "Failed to set SO_REUSEADDR option. " << strerror(errno) << "\n";
      return 1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
      std::cout << "Failed to set SO_REUSEPORT option. " << strerror(errno) << "\n";
      return 1;
    }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";
  std::vector<char> client_buff(MAX_BUFF_SIZE);
  while(1) {
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if (client_fd == -1) return 1;
    std::thread(eventHandler, client_fd).detach();
  }

  close(server_fd);
  return 0;
}
