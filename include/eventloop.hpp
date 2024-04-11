#pragma once

#include <functional>
#include <thread>
#include <condition_variable>

class EventLoop {
public:
	using callable_t = std::function<void()>;

	EventLoop() = default;
	EventLoop(const EventLoop&) = delete;
	EventLoop(EventLoop&&) noexcept = delete;
	~EventLoop() noexcept;

	EventLoop& operator= (const EventLoop&) = delete;
	EventLoop& operator= (EventLoop&&) noexcept = delete;

  void enqueue(callable_t&& callable) noexcept;

private:
	std::vector<callable_t> m_writeBuffer;
	std::mutex m_mutex;
	std::condition_variable m_condVar;
	bool m_running{ true };
	std::thread m_thread{ &EventLoop::threadFunc, this };

  void threadFunc() noexcept;
};
