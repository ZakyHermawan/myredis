#pragma once

#include <string>
#include <unordered_map>

class Database {
private:
  std::unordered_map<std::string, std::string> m_kv;
public:
  Database() = default;
  ~Database() = default;

  void set_value(std::string& key, std::string& val);
  std::string get_value(std::string& key);
};
