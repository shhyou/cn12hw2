CXX       = g++
CXXFLAGS  = -std=c++0x -Wall -Wshadow -Wextra
DEBUG     =
HEADERS   = udt.h rdt.h log.h
OBJS      = udt.o rdt.o log.o
CPPS      = udt.cpp rdt.cpp log.cpp snd.cpp rcv.cpp
TARGET    = sender receiver
#TARGET    =

.PHONY: all clean UDPProxy pack
.SUFFIXES:

all: $(TARGET) $(OBJS)

clean: bucket_clean
	rm -f $(TARGET) $(OBJS) report.log report.aux

UDPProxy:
	gcc UDPProxy.c -o UDPProxy

bucket_clean:
	rm -f bucket/*

%.o: %.cpp $(HEADERS)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(DEBUG)

sender: snd.cpp $(OBJS) $(HEADERS)
	$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS) $(DEBUG)

receiver: rcv.cpp $(OBJS) $(HEADERS)
	$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS) $(DEBUG)

%: %.cpp $(OBJS) $(HEADERS)
	$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS) $(DEBUG)

report.pdf: report.tex
	pdflatex report

pack: $(HEADERS) $(CPPS) report.pdf
	@zip b00902031_b00902107.zip $(HEADERS) $(CPPS) report.pdf Makefile
