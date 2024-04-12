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
#include <shared_mutex>

#include "database.hpp"
#include "eventloop.hpp"
#include "parser.hpp"

#define MAX_BUFF_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

std::shared_ptr<Database> db;

// typedef std::shared_mutex Lock;
// typedef std::unique_lock< Lock >  WriteLock; // C++ 11
// typedef std::shared_lock< Lock >  ReadLock;  // C++ 14

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

            size_t num_bytes = buff.getNextChar() - '0';
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
          db->set_value(arr[1], arr[2]);
          response = "+OK\r\n";
        }
        else if(arr[0] == "get") {
          // do get operation
          std::shared_lock<std::shared_mutex> read_lock(db_mutex);
          std::string val = db->get_value(arr[1]);
          if(val == "") {
            response = "$-1\r\n";
          }
          else {
            response = "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
          }
        }
        send(client_fd, response.c_str(), response.size(), 0);
      }
    }
  }
}

int main(int argc, char **argv) {
  int port = 6379;
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
  db = std::make_shared<Database>();

  while(1) {
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if (client_fd == -1) return 1;
    pid_t pid = fork();
    if(pid == 0) {
      eventHandler(client_fd);
      close(client_fd);
      exit(0);
    }
    else {
      close(client_fd);
    }
  }

  close(server_fd);
  return 0;
}
