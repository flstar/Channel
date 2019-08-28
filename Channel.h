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

	/** @brief
	 * Send an element to the channel.
	 *
	 * If the channel is overfull (more capacity_) including current element,
	 * the sender will be blocked until the channel is back to no more than capacity_ elements.
	 *
	 * If a sender is being blocked then the channel is closed, the sender will return immediately
	 * but the element sent is still available in channel.
	 */
	void send(const T &t)
	{
		std::unique_lock<std::mutex> locker(m_);
		if (closed_) {
			throw ClosedChannelException();
		}
		// Push item even if the queue is full/overfull
		q_.push(t);
		rcv_.notify_one();

		// if queue is overfull, sender creates a cv and wait on it until it is waken up
		if (q_.size() > capacity_) {
			std::shared_ptr<std::condition_variable> scv (new std::condition_variable());
			scvq_.push(scv);
			scv->wait(locker);
		}
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
			while (!scvq_.empty()) {
				scvq_.front()->notify_one();
				scvq_.pop();
			}
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

		// If scvq_ is not empty, wake up and pop the first one
		if (!scvq_.empty()) {
			scvq_.front()->notify_one();
			scvq_.pop();
		}

		return t;
	}

protected:
	std::mutex m_;
	std::condition_variable rcv_;		// cond var receivers wait on
	std::queue<std::shared_ptr<std::condition_variable>> scvq_;

	std::queue<T> q_;
	size_t capacity_;
	bool closed_;
};

#endif
