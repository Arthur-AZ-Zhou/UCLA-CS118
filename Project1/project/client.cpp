#include "consts.h"
#include "io.h"
#include "transport.h"
#include <arpa/inet.h>
#include <iostream>  // Use C++ input/output
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage: client <hostname> <port>" << endl;
        exit(1);
    }

    /* Create sockets */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    /* Construct server address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    string addr = (strcmp(argv[1], "localhost") == 0) ? "127.0.0.1" : argv[1];
    server_addr.sin_addr.s_addr = inet_addr(addr.c_str());
    int PORT = atoi(argv[2]);
    server_addr.sin_port = htons(PORT);

    init_io();
    listen_loop(sockfd, &server_addr, CLIENT_START, input_io, output_io);

    return 0;
}
