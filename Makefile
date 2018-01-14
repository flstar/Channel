.PHONY: default test

default: TestChannel

TestChannel: TestChannel.cxx Channel.h
	$(CXX) -o $@ $<

test: default
	./TestChannel

clean:
	rm -f TestChannel
