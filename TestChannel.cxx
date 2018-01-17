#include <future>
#include <atomic>
#include <gtest/gtest.h>

#include "Channel.h"

using namespace std;


TEST(Channel, SimpleSendRecv)
{
	Channel<int> c;
	
	auto f = async([&c] () {
		for (int i=0; i<100; i++) {
			c.send(i);
		}
		c.close();
	});

	for (int i=0; i<100; i++) {
		EXPECT_EQ(i, c.recv());
	}
	EXPECT_THROW(c.recv(), ClosedChannelException);

	f.get();
}

TEST(Channel, SizedSendRecv)
{
	Channel<int> c(2);
	atomic<bool> r1(false), r2(false);
	
	auto f = async([&c, &r1, &r2] () {
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

	usleep(1);
	r1 = true;
	EXPECT_EQ(1, c.recv());
	
	usleep(1);
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
	
	auto f = async([&c, &r1, &r2] () {
		c.send(1);
		EXPECT_EQ(true, r1);
		c.send(2);
		EXPECT_EQ(true, r2);
		c.close();
	});

	usleep(1);
	r1 = true;
	EXPECT_EQ(1, c.recv());

	usleep(1);
	r2 = true;
	EXPECT_EQ(2, c.recv());

	EXPECT_THROW(c.recv(), ClosedChannelException);

	f.get();

}


int main(int argc, char *argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

