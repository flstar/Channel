#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <queue>
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
	Channel(size_t size = 1024)
	{
		size_ = size;
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
		// Push item even if the queue is full/overfull
		q_.push(t);
		rcv_.notify_one();
		// But we won't return if:
		//  - queue is overfull, and
		//  - channel is still open
		while (!closed_ && q_.size() > size_) {
			scv_.wait(locker);
		}
	}

	T recv()
	{
		std::unique_lock<std::mutex> locker(m_);
		while (q_.empty()) {
			if (closed_) {
				throw ClosedChannelException();
			}
			else {
				rcv_.wait(locker);
			}
		}
		// Remove the head element to return it
		T t = q_.front();
		q_.pop();
		// If queue is back to full from overfull, notify all sends waiting on it
		if (q_.size() <= size_) {
			scv_.notify_all();
		}
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
			rcv_.notify_all();
			scv_.notify_all();
		}
	}

protected:
	std::mutex m_;
	std::condition_variable rcv_;		// cond var reciever waits on
	std::condition_variable scv_;		// cond var sender waits on

	std::queue<T> q_;
	size_t size_;
	bool closed_;
};

#endif