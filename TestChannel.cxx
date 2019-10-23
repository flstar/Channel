#include <future>
#include <atomic>
#include <gtest/gtest.h>

#include "Channel.h"

using namespace std;


TEST(Channel, SimpleSendRecv)
{
	Channel<int> c;

	auto f = async(launch::async, [&c] () {
		for (int i=0; i<1000; i++) {
			c.send(i);
		}
		c.close();
	});

	for (int i=0; i<1000; i++) {
		EXPECT_EQ(i, c.recv());
	}
	EXPECT_THROW(c.recv(), ClosedChannelException);

	f.get();
}

TEST(Channel, SizedSendRecv)
{
	Channel<int> c(2);
	atomic<bool> r1(false), r2(false);

	auto f = async(launch::async, [&c, &r1, &r2] () {
		c.send(1);
		c.send(2);

		// should blocked at this line until 1 is recieved
		c.send(3);
		EXPECT_EQ(true, r1);

		// should blocked at this line until 1 is recieved
		c.send(4);
		EXPECT_EQ(true, r2);
		c.close();
	});

	usleep(1000);
	r1 = true;
	EXPECT_EQ(1, c.recv());

	usleep(1000);
	r2 = true;
	EXPECT_EQ(2, c.recv());

	EXPECT_EQ(3, c.recv());
	EXPECT_EQ(4, c.recv());
	EXPECT_THROW(c.recv(), ClosedChannelException);

	f.get();
}

TEST(Channel, SyncSendRecv)
{
	Channel<int> c(0);
	atomic<bool> r1(false), r2(false);

	auto f = async(launch::async, [&c, &r1, &r2] () {
		c.send(1);
		EXPECT_EQ(true, r1);
		c.send(2);
		EXPECT_EQ(true, r2);
		c.close();
		printf("Channel closed!\n");
	});

	usleep(1000);
	r1 = true;
	EXPECT_EQ(1, c.recv());

	usleep(1000);
	r2 = true;
	EXPECT_EQ(2, c.recv());

	usleep(1000);
	EXPECT_THROW(c.recv(), ClosedChannelException);

	f.get();

}

TEST(Channel, Operators)
{
	Channel<int> c;

	auto f = async(launch::async, [&c] () {
		c << 1 << 2 << 3;
		c.close();
	});

	int x1, x2, x3, x4;
	usleep(10*1000);
	c >> x1 >> x2 >> x3;
	EXPECT_EQ(1, x1);
	EXPECT_EQ(2, x2);
	EXPECT_EQ(3, x3);
	usleep(10*1000);
	EXPECT_THROW(c >> x4, ClosedChannelException);

	f.get();
}

TEST(Channel, CloseNotifyAndException)
{
	Channel<int> c(0);

	auto f = async(launch::async, [&c] () {
		EXPECT_THROW(c.send(1), ClosedChannelException);
		EXPECT_THROW(c.close(), ClosedChannelException);
	});

	usleep(1000);
	c.close();

	EXPECT_THROW(c.recv(), ClosedChannelException);
	EXPECT_THROW(c.close(), ClosedChannelException);

	f.get();
}

TEST(Channel, BlockMultiSenders)
{
	Channel<int> c(0);
	atomic<bool> r1(false), r2(false);

	auto f1 = async(launch::async, [&] () {
		c.send(1);
		EXPECT_EQ(true, r1);
	});

	auto f2 = async(launch::async, [&] () {
		usleep(1000);
		c.send(2);
		EXPECT_EQ(true, r2);
	});

	usleep(2000);
	r1 = true;
	EXPECT_EQ(1, c.recv());

	r2 = true;
	EXPECT_EQ(2, c.recv());

	f1.get();
	f2.get();
}

TEST(Channel, TryRecv)
{
	Channel<int> c;
	int x;

	EXPECT_FALSE(c.try_recv(&x));		// timeout immediately

	c.send(1);
	c.recv();

	auto start = std::chrono::high_resolution_clock::now();
	EXPECT_FALSE(c.try_recv(&x, 100*1000));
	auto end = std::chrono::high_resolution_clock::now();

	EXPECT_GT(end - start, std::chrono::microseconds(90*1000));

	c.close();
	EXPECT_THROW(c.try_recv(nullptr, 1000), ClosedChannelException);
}

TEST(Channel, TrySend)
{
	Channel<int> c(1);

	EXPECT_TRUE(c.try_send(1, 10));
	EXPECT_FALSE(c.try_send(2, 10));

	auto f1 = async(launch::async, [&] () {
		EXPECT_TRUE(c.try_send(3, 10*1000000));
	});

	EXPECT_EQ(1, c.recv());
	EXPECT_EQ(3, c.recv());
}

int main(int argc, char *argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
