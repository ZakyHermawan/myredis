#include "database.hpp"

void Database::set_value(std::string& key, std::string& val) {
  m_kv[key] = val;
  set_expiry(key, std::chrono::milliseconds(0));
}

void Database::set_expiry(std::string& key, ms_type expiry) {
  m_expiry[key] = std::make_pair<ms_type, system_time_point>(
      std::move(expiry),
      std::chrono::system_clock::now()
    );
}

std::string Database::get_value(std::string& key) {
  return m_kv[key];
}

ms_type Database::get_expiry(std::string& key) {
  return m_expiry[key].first;
}

system_time_point Database::get_timestamp(std::string& key) {
  return m_expiry[key].second;
}
