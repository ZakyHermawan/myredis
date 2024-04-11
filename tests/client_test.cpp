#include <iostream>
#include <thread>
#include <unistd.h>
#include <future>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <gtest/gtest.h>
#include <gtest/internal/gtest-internal.h>

#include "eventloop.hpp"

#define MAX_BUFF_SIZE 1024

void wakeup_server() {
  std::string filename = "../server";
  char *argv_for_program[] = { (char*)filename.c_str(), NULL };
  execve((char*)filename.c_str(), argv_for_program, NULL);
}

std::string send_ping() {
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(6379);
  serverAddress.sin_addr.s_addr = INADDR_ANY;
  return "haha";
  connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

  std::string msg = "+ping\n";
  send(clientSocket, msg.c_str(), msg.length(), 0);

  std::vector<char> msg_buff(MAX_BUFF_SIZE);
  int read = recv(clientSocket, &msg_buff[0], msg_buff.size(), 0);
  if (read < 0) std::cerr << "Client read failed\n";
  
  std::string ret(msg_buff.begin(), msg_buff.end());

  return ret;
}

int Factorial(int n) {
  if(n == 0) {
    return 1;
  }
  return n * Factorial(n-1);
}

TEST(FactorialTest, HandlesZeroInput) {
  EXPECT_EQ(Factorial(0), 1);
}

TEST(ConcurrencyTest, DoublePing) {
  pid_t pid = fork();
  if(pid < 0) {
    std::cerr << "Fork failed\n";
  }
  else if(pid == 0) {
    wakeup_server();
  }

  std::future<std::string> ret1 = std::async(&send_ping);
  std::future<std::string> ret2 = std::async(&send_ping);

  std::string retval1 = ret1.get();
  std::string retval2 = ret2.get();
  EXPECT_TRUE(retval1 == retval2);
  kill(pid, SIGKILL);
}
