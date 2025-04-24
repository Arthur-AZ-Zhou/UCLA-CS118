CC = g++
CPPFLAGS = -Wall -Wextra
LDFLAGS = 
LDLIBS = 

DEPS = transport.o io.o

# Targets for compiling and linking
all: server client 

# Server target
server: server.o $(DEPS)
	$(CC) $(LDFLAGS) $^ -o $@

# Client target
client: client.o $(DEPS)
	$(CC) $(LDFLAGS) $^ -o $@

# Rule for compiling .cpp files to .o
%.o: %.cpp
	$(CC) $(CPPFLAGS) -c $< -o $@

# Clean up object files and executables
clean:
	@rm -rf server client *.bin *.o
