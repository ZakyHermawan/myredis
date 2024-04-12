#include "database.hpp"

void Database::set_value(std::string& key, std::string& val) {
  m_kv[key] = val;
}

std::string Database::get_value(std::string& key) {
  return m_kv[key];
}
