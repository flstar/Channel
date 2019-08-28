.PHONY: default test

CXX = g++ -std=c++11
CXXFLAGS = -g -MMD -Wall

TARGET = TestChannel
SRCS = TestChannel.cxx
OBJS = $(SRCS:.cxx=.o)
DEPS = $(OBJS:.o=.d)

LIBS = -L/usr/local/lib -lgtest -lpthread

default: $(TARGET)

-include $(DEPS)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

%.o: %.cxx
	$(CXX) $(CXXFLAGS) -o $@ -c $<

test: default
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

clobber: clean
	rm -rf $(DEPS)
