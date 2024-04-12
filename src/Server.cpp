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

#include "database.hpp"
#include "eventloop.hpp"
#include "parser.hpp"

#define MAX_BUFF_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

std::shared_ptr<Database> db;
std::shared_mutex db_mutex;

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
          response = "$" + std::to_string(arr[1].length()) + "\r\n" + arr[1] + "\r\n";
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
              response = "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
            }
            else {
              response = "$-1\r\n";
            }
          }
        }
        send(client_fd, response.c_str(), response.size(), 0);
      }
    }
  }
}

int main(int argc, char **argv) {
  db = std::make_shared<Database>();

  int port;
  if(argc < 2) {
    port = 6379;
  }
  else {
    sscanf(argv[2], "%d", &port);
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
