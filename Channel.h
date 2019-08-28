#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <queue>
#include <mutex>


class ClosedChannelException : std::exception
{
public:
	virtual const char *what() const noexcept override {
		return "Channel was closed";
	}
};

class TimeoutException : std::exception
{
public:
	virtual const char *what() const noexcept override {
		return "Timeout";
	}
};

class InternalErrorException : std::exception
{
public:
	virtual const char *what() const noexcept override {
		return "Internal error";
	}
};

template <typename T>
class Channel
{
public:
	explicit Channel(size_t capacity = 1024)
	{
		capacity_ = capacity;
		closed_ = false;
	}
	virtual ~Channel() {}

	Channel (const Channel &) = delete;
	Channel & operator = (const Channel &) = delete;

	void send(const T &t)
	{
		std::unique_lock<std::mutex> locker(m_);
		if (closed_) {
			throw ClosedChannelException();
		}
		// Push item even if the queue is full/overfull
		q_.push(t);
		if (q_.size() == 1) {
			rcv_.notify_one();
		}
		// But we will hold on here unless:
		//  - queue is not overfull, or
		//  - channel is closed
		scv_.wait(locker, [&] () { return closed_ || q_.size() <= capacity_; } );
	}
	/** @brief
	 * After a channel is closed, data could not be sent to it any more. But:
	 *   - existing data is still available for recv()
	 *   - for senders being blocked at send(), they will be unblocked immediately.
	 *     The data sent by these senders is still available to receivers
	 */
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

	T recv()
	{
		std::unique_lock<std::mutex> locker(m_);
		rcv_.wait(locker, [&] () { return closed_ || !q_.empty(); } );
		if (!q_.empty()) {
			return _recv_locked();
		}
		else if (closed_) {
			throw ClosedChannelException();
		}
		else {
			throw InternalErrorException();
		}
	}

	T try_recv_timeout(uint64_t timeout_us = 0)
	{
		std::unique_lock<std::mutex> locker(m_);

		rcv_.wait_for(
			locker,
			std::chrono::microseconds(timeout_us),
			[&] () { return closed_ || !q_.empty(); }
		);

		if (!q_.empty()) {
			return _recv_locked();
		}
		else if (closed_) {
			throw ClosedChannelException();
		}
		else {
			throw TimeoutException();
		}
	}

	Channel & operator << (const T &t) {
		send(t);
		return *this;
	}

	Channel & operator >> (T &t) {
		t = recv();
		return *this;
	}

protected:
	T _recv_locked()
	{
		T t = q_.front();
		q_.pop();

		// If queue is back to full from overfull, notify all senders waiting on it
		if (q_.size() == capacity_) {
			scv_.notify_all();
		}

		return t;
	}

protected:
	std::mutex m_;
	std::condition_variable rcv_;		// cond var receivers wait on
	std::condition_variable scv_;		// cond var senders wait on

	std::queue<T> q_;
	size_t capacity_;
	bool closed_;
};

#endif
