CXX       = g++
CXXFLAGS  = -std=c++0x -Wall -Wshadow -Wextra -I.. -DTEST
HEADERS   = ../udt.h ../rdt.h ../log.h
OBJS      = udt.o fake_rdt.o log.o
TARGET    = snd rcv
#TARGET    =

.PHONY: all clean test_snd test_rcv
.SUFFIXES:

all: $(TARGET) $(OBJS)

clean:
	@rm -f $(TARGET) $(OBJS)
	@rm -f bucket/*

test_snd: snd
	./snd localhost 18018 ../rdt.cpp

test_rcv: rcv
	./rcv 18018

%.o: ../%.cpp $(HEADERS)
	@$(CXX) -c -o $@ $< $(CXXFLAGS)

fake_rdt.o: fake_rdt.cpp $(HEADERS)
	@$(CXX) -c -o $@ $< $(CXXFLAGS)

%: ../%.cpp $(OBJS) $(HEADERS)
	@$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS)

