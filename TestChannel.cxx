#include <thread>
#include <iostream>
#include <future>
#include "Channel.h"

using namespace std;

Channel<int> c;

void f1(Channel<int> &c)
{
	try {
		while (true) {
			int x = c.recv();
			cout<<"Receive "<<x<<endl;
		}
	} catch (ClosedChannelException ex) {
		cout<<ex.what()<<endl;
	}
}


int main()
{
	auto f = std::async(f1, std::ref(c));
	c.send(1);
	c.send(2);
	c.close();
	
	f.get();
	
	return 0;
}

