CC = g++
CFLAGS = -std=c++11 -Wall -g

SRCS = client.cpp server.cpp io.cpp transport.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(OBJS)
	$(CC) $(OBJS) -o app

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) app
