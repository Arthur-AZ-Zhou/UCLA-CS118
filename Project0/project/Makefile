CXX=g++
CPPFLAGS=-Wall -Wextra
LDFLAGS=

DEPS=

all: server client

server: server.o $(DEPS)
	$(CXX) server.o -o server $(LDFLAGS)

client: client.o $(DEPS)
	$(CXX) client.o -o client $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) -c -o $@ $<

clean:
	@rm -rf server client *.bin *.o
