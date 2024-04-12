#pragma once

#include <string>
#include <chrono>
#include <utility>
#include <unordered_map>

#include <iostream>

using ms_type = std::chrono::duration<size_t, std::ratio<1, 1000>>;
using system_time_point = std::chrono::time_point<std::chrono::system_clock>;
// example usage: ms_type duration_ms duration_ok{3} 
// const std::chrono::time_point<std::chrono::system_clock> now =
//     std::chrono::system_clock::now();

class Database {
public:
  std::unordered_map<std::string, std::string> m_kv;

  // key, (expiry in ms, timestamp where data being stored)  
  std::unordered_map<std::string, 
    std::pair<
      ms_type, 
      system_time_point
    >
  > m_expiry;

public:
  Database() = default;
  ~Database() = default;

  void set_value(std::string& key, std::string& val);
  void set_expiry(std::string& key, ms_type expiry);
  std::string get_value(std::string& key);
  ms_type get_expiry(std::string& key);
  system_time_point get_timestamp(std::string& key);

  size_t get_size();
};
