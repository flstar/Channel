.PHONY: default test

LIBS = -L/usr/local/lib -lgtest
default: TestChannel

TestChannel: TestChannel.cxx Channel.h
	$(CXX) -o $@ $< $(LIBS)

test: default
	./TestChannel

clean:
	rm -f TestChannel
