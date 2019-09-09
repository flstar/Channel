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
	 * If chanel is closed while a sender is being blocked, the sender will return immediately
	 * but the element is left available in channel.
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

		// if queue is overfull, sender creates a cv and wait on it
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
			// wake up all blocked senders
			while (!scvq_.empty()) {
				scvq_.front()->notify_one();
				scvq_.pop();
			}
		}
	}

	T recv()
	{
		T t;
		bool succ = false;
		while (!succ) {
			succ = try_recv(&t, 3600*1000000);
		}
		return std::move(t);
	}

	/** @brief
	 * Try to retrieve one element from channel.
	 * If the channel is empty, wait for at most timeout_us micro-seconds.
	 * If the channel is empty and closed, throw CloseChannelException()
	 * @return
	 *   Return true if retrieve 1 element successfully.
	 *   Return false if timeout.
	 */
	bool try_recv(T *t, uint64_t timeout_us = 0)
	{
		std::unique_lock<std::mutex> locker(m_);

		rcv_.wait_for(
			locker,
			std::chrono::microseconds(timeout_us),
			[&] () { return !q_.empty() || closed_; }
		);

		if (!q_.empty()) {
			_recv_locked(t);
			return true;
		}
		else if (closed_) {
			throw ClosedChannelException();
		}
		else {	// timeout
			return false;
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
	void _recv_locked(T *t)
	{
		if (t != nullptr) {
			*t = q_.front();
		}
		q_.pop();

		// If there is any sender being blocked in scvq_, notify the first one ane pop it
		if (!scvq_.empty()) {
			scvq_.front()->notify_one();
			scvq_.pop();
		}

		return;
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
