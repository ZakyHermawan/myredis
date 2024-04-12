#pragma once

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
