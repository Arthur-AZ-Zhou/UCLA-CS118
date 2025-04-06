#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h> // strlen
#include <errno.h> // Get error number
using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: server <port>\n";
        return 1;
    }

    int port = std::atoi(argv[1]);

    // TODO: Create socket

    // TODO: Set stdin and socket nonblocking

    // TODO: Construct server address

    // TODO: Bind address to socket

    // TODO: Create sockaddr_in and socklen_t buffers to store client address

    char buffer[1024];
    bool client_connected = false;

    // Listen loop
    while (true) {
        // TODO: Receive from socket
        // TODO: If no data and client not connected, continue
        // TODO: If data, client is connected and write to stdout
        // TODO: Read from stdin
        // TODO: If data, send to socket
    }

    return 0;
}
