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
    int port = atoi(argv[2]);

    // TODO: Create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                     // use IPv4  use UDP
    
    if (sockfd < 0) {
        cerr << "Socket must be 1 or greater: " << strerror(errno) << endl; 
        return 1;
    }

    //SOCKET NONBLOCKING
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);

    //STDIN NONBLOCKING
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flags);

    // TODO: Construct server address
    struct sockaddr_in serveraddr = {0};
    serveraddr.sin_family = AF_INET; // use IPv4
    serveraddr.sin_addr.s_addr = INADDR_ANY;

    // Set receiving port
    serveraddr.sin_port = htons(port); // Big endian
    cerr << "Server address configured for port " << port << endl;

    int BUF_SIZE = 1024;
    char server_buf[BUF_SIZE];
    socklen_t serversize = sizeof(serveraddr);

    cerr << "Connected to server at " << addr << ":" << port << endl;

    // Listen loop
    while (true) {
        // Read from stdin
        int bytes_recvd = recvfrom(sockfd, server_buf, BUF_SIZE, 0, (struct sockaddr*) &serveraddr, &serversize);

        char* server_ip = inet_ntoa(serveraddr.sin_addr);
        int server_port = ntohs(serveraddr.sin_port);

        write(1, server_buf, bytes_recvd);
        ssize_t read_len = read(0, server_buf, BUF_SIZE);

        if (read_len > 0) {
            ssize_t did_send = sendto(sockfd, server_buf, read_len, 0, (struct sockaddr*)&serveraddr, serversize);
        } 
    }

    close(sockfd);
    return 0;
}
