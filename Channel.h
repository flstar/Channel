#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <queue>
#include <mutex>


class ClosedChannelException : public std::exception
{
public:
	virtual const char *what() const noexcept override {
		return "Channel was closed!";
	}
};


template <typename T>
class Channel
{
public:
	explicit Channel(size_t capacity = 1024)
	{
		sync_slots_ = 0;
		capacity_ = capacity;
		closed_ = false;
	}
	virtual ~Channel() {}

	Channel (const Channel &) = delete;
	Channel & operator = (const Channel &) = delete;


	bool _try_send_unbuffered(const T &t, uint64_t timeout_us)
	{
		std::unique_lock<std::mutex> locker(mtx_);

		scv_.wait_for(
			locker,
			std::chrono::microseconds(timeout_us),
			[&] () { return sync_slots_ > 0 || closed_; }
		);

		if (closed_) {
			throw ClosedChannelException();
		}
		else if (sync_slots_ > 0) {
			q_.push(t);
			-- sync_slots_;
			rcv_.notify_one();
			return true;
		}
		else {
			return false;
		}

	}
	/**
	 * Try to send an element to the channel in timeout_us micro-seconds.
	 *
	 * Return true if the element is sent successfully. Or return false in case of timeout.
	 *
	 * If the channel is closed while waiting, ClosedChannelException will be thrown out. The element was NOT sent.
	 */
	bool try_send(const T &t, uint64_t timeout_us = 0)
	{
		if (capacity_ == 0) {
			return _try_send_unbuffered(t, timeout_us);
		}

		std::unique_lock<std::mutex> locker(mtx_);

		scv_.wait_for(
			locker,
			std::chrono::microseconds(timeout_us),
			[&] () { return q_.size() < capacity_ || closed_; }
		);

		if (closed_) {
			throw ClosedChannelException();
		}
		else if (q_.size() < capacity_ ) {
			q_.push(t);
			rcv_.notify_one();
			return true;
		}
		else {
			return false;
		}
	}

	/** @brief
	 * Send an element to the channel.
	 */
	inline void send(const T &t)
	{
		while (!try_send(t, 600*1000000));
	}
	/** @brief
	 * After a channel is closed, data could not be sent to it any more. But:
	 *   - existing data is still available for recv()
	 *   - for senders being blocked at send(), they will be unblocked immediately.
	 *     The data sent by these senders is still available to receivers
	 */
	void close()
	{
		std::unique_lock<std::mutex> locker(mtx_);
		if (closed_) {
			throw ClosedChannelException();
		}
		else {
			closed_ = true;
			rcv_.notify_all();
			// wake up all blocked senders
			scv_.notify_all();
		}
	}

	inline T recv()
	{
		T t;
		bool succ = false;
		while (!succ) {
			succ = try_recv(&t, 600*1000000);
		}
		return std::move(t);
	}

	bool _try_recv_unbuffered(T *t, uint64_t timeout_us)
	{
		std::unique_lock<std::mutex> locker(mtx_);
		++ sync_slots_;
		scv_.notify_one();

		rcv_.wait_for(
			locker,
			std::chrono::microseconds(timeout_us),
			[&] () { return !q_.empty() || closed_; }
		);

		if (!q_.empty()) {
			if (t != nullptr) {
				*t = q_.front();
			}
			q_.pop();
			return true;
		}
		else if (closed_) {
			throw ClosedChannelException();
		}
		else {
			return false;
		}
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
		if (capacity_ == 0) {
			return _try_recv_unbuffered(t, timeout_us);
		}

		std::unique_lock<std::mutex> locker(mtx_);

		rcv_.wait_for(
			locker,
			std::chrono::microseconds(timeout_us),
			[&] () { return !q_.empty() || closed_; }
		);

		if (!q_.empty()) {
			if (t != nullptr) {
				*t = q_.front();
			}
			q_.pop();
			scv_.notify_one();
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
	std::mutex mtx_;
	std::condition_variable rcv_;		// cond var receivers wait on
	std::condition_variable scv_;		// cond var senders wait on

	std::queue<T> q_;
	size_t capacity_;
	bool closed_;

	size_t sync_slots_;					// how many receivers are waiting for unbuffered channel
};

#endif
