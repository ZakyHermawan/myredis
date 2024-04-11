#include <iostream>

#include "eventloop.hpp"

EventLoop::~EventLoop() noexcept {
  enqueue([this]
  {
    m_running = false;
  });
  m_thread.join();
}

void EventLoop::enqueue(callable_t&& callable) noexcept {
		{
			std::lock_guard<std::mutex> guard(m_mutex);
			m_writeBuffer.emplace_back(std::move(callable));
		}
}

void EventLoop::threadFunc() noexcept {
  std::vector<callable_t> readBuffer;
		while (m_running){
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_condVar.wait(lock, [this]
				{
					return !m_writeBuffer.empty();
				});
				std::swap(readBuffer, m_writeBuffer);
			}
			
			for (callable_t& func : readBuffer)
			{
				func();
			}
			
			readBuffer.clear();
		}

}
