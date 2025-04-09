CC=gcc
CFLAGS=-Wall -Wextra
LDFLAGS=

DEPS=

all: server client

server: server.o $(DEPS)
	$(CC) server.o -o server $(LDFLAGS)

client: client.o $(DEPS)
	$(CC) client.o -o client $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@rm -rf server client *.bin *.o
