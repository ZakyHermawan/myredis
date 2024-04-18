#include <iostream>
#include <cstring>
#include "parser.hpp"

Buffer::Buffer(std::string& buff) {
  std::copy(buff.begin(), buff.end(), std::back_inserter(m_buff));
}

Buffer::Buffer(char* buff) {
  for(size_t i=0; i<strlen(buff); ++i) {
    if(i == 0) {
      m_next_char = buff[i];
    }
    m_buff.push_back(buff[i]);
  }
}

char Buffer::getNextChar() {
  if(m_next_idx == m_buff.size()) {
    std::cerr << "buffer index out of bound\n";
    exit(1);
  }
  char curr_char = m_next_char;
  ++m_next_idx;
  if(m_next_idx != m_buff.size()) {
    m_next_char = m_buff[m_next_idx];
  }

  return curr_char;
}

char Buffer::lookNextChar() {
  return m_next_char;
}

void Buffer::remove_clrf() {
  getNextChar();
  getNextChar();
}

std::string compose_bulk_string(std::string& payload) {
  return "$" + std::to_string(payload.length()) + "\r\n" + payload + "\r\n";
}

std::string compose_array(std::vector<std::string> data) {
  std::string raw_str = "*" + std::to_string(data.size()) + "\r\n";
  for(auto d: data) {
    raw_str += "$" + std::to_string(d.length()) + "\r\n" + d + "\r\n";
  }
  return raw_str;
}
