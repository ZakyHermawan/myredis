#pragma once

#include <unordered_map>
#include <vector>

class Buffer {
private:
  size_t m_max_buff_size { 1024 };
  std::vector<char> m_buff;
  
  // for lookahead purpose
  size_t m_next_idx{ 0 };
  char m_next_char;
public:
  Buffer() = default;
  Buffer(std::string& buff);
  Buffer(char* buff);
  ~Buffer() = default;

  char getNextChar();
  char lookNextChar();
  void remove_clrf();
};

std::string compose_bulk_string(std::string& payload);
std::string compose_array(std::vector<std::string>);
std::string convert_to_binary(std::string hex);
