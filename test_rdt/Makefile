FLAGS = -std=c++0x -Wall -Wshadow -Wextra
CPPS = ../udt.cpp ../log.cpp ../rdt.cpp
HDRS = ../udt.h ../log.h ../rdt.h
.PHONY: clean
all: client server
client: client.cpp $(HDRS) $(CPPS)
	g++ client.cpp $(CPPS) -o client $(FLAGS)
server: server.cpp $(HDRS) $(CPPS)
	g++ server.cpp $(CPPS) -o server $(FLAGS)
clean:
	@rm -f server client
