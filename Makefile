CXX       = g++
CXXFLAGS  = -std=c++0x
HEADERS   = udt.h rdt.h log.h
OBJS      = udt.o rdt.o log.o
TARGET    = snd rcv
#TARGET    =

.PHONY: all clean
.SUFFIXES:

all: $(TARGET) $(OBJS)

clean:
	rm -f $(TARGET) $(OBJS)

%.o: %.cpp $(HEADERS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

%: %.cpp $(OBJS) $(HEADERS)
	$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS)

