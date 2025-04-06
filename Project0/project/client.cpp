#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h> // strlen
#include <errno.h> // Get error number
using namespace std;

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage: client <hostname> <port>\n";
        return 1;
    }

    // Only supports localhost as a hostname, but that's all we'll test on
    const char* addr = (strcmp(argv[1], "localhost") == 0) ? "127.0.0.1" : argv[1];
    int port = std::atoi(argv[2]);

    // TODO: Create socket

    // TODO: Set stdin and socket nonblocking

    // TODO: Construct server address

    char buffer[1024];

    // Listen loop
    while (true) {
        // TODO: Receive from socket
        // TODO: If data, write to stdout
        // TODO: Read from stdin
        // TODO: If data, send to socket
    }

    return 0;
}
