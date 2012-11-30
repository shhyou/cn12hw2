.PHONY: clean
all: client server
client: client.cpp ../udt.h ../udt.cpp
	g++ client.cpp ../udt.cpp ../log.cpp -o client
server: server.cpp ../udt.h ../udt.cpp
	g++ server.cpp ../udt.cpp ../log.cpp -o server
clean:
	@rm -f server client
