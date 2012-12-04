CXX       = g++
CXXFLAGS  = -std=c++0x -Wall -Wshadow -Wextra
HEADERS   = udt.h rdt.h log.h
OBJS      = udt.o rdt.o log.o
TARGET    = snd rcv sender receiver
#TARGET    =

.PHONY: all clean UDPProxy
.SUFFIXES:

all: $(TARGET) $(OBJS)

clean: bucket_clean
	rm -f $(TARGET) $(OBJS) report.log report.aux

UDPProxy:
	gcc UDPProxy.c -o UDPProxy

bucket_clean:
	rm -f bucket/*

%.o: %.cpp $(HEADERS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

sender: snd.cpp $(OBJS) $(HEADERS)
	$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS)

receiver: snd.cpp $(OBJS) $(HEADERS)
	$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS)

%: %.cpp $(OBJS) $(HEADERS)

