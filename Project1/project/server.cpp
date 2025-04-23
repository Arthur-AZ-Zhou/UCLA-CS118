#include "consts.h"
#include "transport.h"
#include "io.h"
#include <arpa/inet.h>
#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: server <port>" << std::endl;
        exit(1);
    }

    /* Create sockets */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    /* Construct our address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int PORT = atoi(argv[1]);
    server_addr.sin_port = htons(PORT);

    bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    struct sockaddr_in client_addr;
    socklen_t s = sizeof(struct sockaddr_in);
    char buffer;

    recvfrom(sockfd, &buffer, sizeof(buffer), MSG_PEEK,
        (struct sockaddr*)&client_addr, &s);

    init_io();
    listen_loop(sockfd, &client_addr, SERVER_AWAIT, input_io, output_io);

    return 0;
}
