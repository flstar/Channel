#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <queue>
#include <atomic>
#include <mutex>

class ClosedChannelException : std::exception
{
public:
	virtual const char *what() const noexcept override {
		return "Channel is closed!";
	}
};

template <typename T>	
class Channel
{
public:
	Channel()
	{
		closed_ = false;
	}
	
	Channel (const Channel &) = delete;
	Channel & operator = (const Channel &) = delete;
	
	virtual ~Channel()
	{
	}

	void send(const T &t)
	{
		std::unique_lock<std::mutex> locker(m_);
		if (closed_) {
			throw ClosedChannelException();
		}
		q_.push(t);
		cv_.notify_one();
	}

	T recv()
	{
		std::unique_lock<std::mutex> locker(m_);
		while (q_.empty()) {
			if (closed_) {
				throw ClosedChannelException();
			}
			else {
				cv_.wait(locker);
			}
		}
		// Remove the head element and return it
		T t = q_.front();
		q_.pop();
		return t;
	}

	void close()
	{
		std::unique_lock<std::mutex> locker(m_);
		if (closed_) {
			throw ClosedChannelException();
		}
		else {
			closed_ = true;
			cv_.notify_all();
		}
	}
 
protected:
	std::queue<T> q_;
	
	std::mutex m_;
	std::condition_variable cv_;
	bool closed_;
};

#endif