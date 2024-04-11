#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }

int main(int argc, char **argv) {
  std::cout << "Logs from your program will appear here!\n";
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
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
  
  int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  if(client_fd < 0) {
    std::cout << "Client fail to connect\n";
  }
  std::cout << "Client connected\n";
  char client_buff[BUFFER_SIZE];
  while(1) {
    int read = recv(client_fd, client_buff, BUFFER_SIZE, 0);
    if (!read) break; // done reading
    if (read < 0) on_error("Client read failed\n");
    
    if(strcmp(client_buff, "*1\r\n$4\r\nping\r\n") == 0) {
      std::string reply = "+PONG\r\n";

      int err = send(client_fd, reply.c_str(), pong.size(), 0);
      if (err < 0) std::cerr << "Client write failed\n";
    }
    else {
      std::cerr << "unknown command\n";
      close(server_fd);
      return 1;
    }
  }

  close(server_fd);
  return 0;
}
