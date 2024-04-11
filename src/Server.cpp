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

#include "eventloop.hpp"

#define MAX_BUFF_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

void reply_ping(int client_fd) {
  while(true) {
    char buffer[MAX_BUFF_SIZE] = "";
    while (recv(client_fd, buffer, MAX_BUFF_SIZE, 0)) {
      std::string request(buffer);
      if (request.find("ping\r\n") != std::string::npos) {
        std::string response = "+PONG\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        request.erase(request.find("ping\r\n"), 6);
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
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cout << "Failed to set SO_REUSEADDR option. " << strerror(errno) << "\n";
        return 1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
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
  std::vector<std::thread> threads;

  while(1) {
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if (client_fd == -1) return 1;
    pid_t pid = fork();
    if(pid == 0) {
      reply_ping(client_fd);
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
