cmake_minimum_required(VERSION 3.13)
project(redis-starter-cpp)
set(CMAKE_CXX_STANDARD 20) # Enable the C++20 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

##############
# External Library
##############

# add_subdirectory(external/gtest)

##############
# Tests
##############

# ENABLE_TESTING()
# ADD_SUBDIRECTORY(tests)

add_executable(
  server
  src/Server.cpp
  src/database.cpp
  src/eventloop.cpp
  src/parser.cpp
)
target_include_directories(server PRIVATE include)
target_link_libraries(server pthread)
