#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     // strlen, strcmp
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>      // strerror
#include <stdbool.h>    // for bool type

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: client <hostname> <port>\n");
        return 1;
    }

    // Only supports localhost as a hostname
    const char* addr = (strcmp(argv[1], "localhost") == 0) ? "127.0.0.1" : argv[1];
    int port = atoi(argv[2]);

    // Create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Socket must be 1 or greater: %s\n", strerror(errno));
        return 1;
    }

    // SOCKET NONBLOCKING
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);

    // STDIN NONBLOCKING
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flags);

    // Construct server address
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr, &serveraddr.sin_addr) <= 0) {
        close(sockfd);
        return 1;
    }

    int BUF_SIZE = 1024;
    char server_buf[BUF_SIZE];
    socklen_t serversize = sizeof(serveraddr);

    // Listen loop
    while (1) {
        int bytes_recvd = recvfrom(sockfd, server_buf, BUF_SIZE, 0,
                                   (struct sockaddr*)&serveraddr, &serversize);

        if (bytes_recvd > 0) {
            write(STDOUT_FILENO, server_buf, bytes_recvd);
        }

        ssize_t read_len = read(STDIN_FILENO, server_buf, BUF_SIZE);
        if (read_len > 0) {
            sendto(sockfd, server_buf, read_len, 0,
                   (struct sockaddr*)&serveraddr, serversize);
        }
    }

    close(sockfd);
    return 0;
}
