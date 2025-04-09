#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     // strlen
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>      // strerror
#include <stdbool.h>    // for bool type

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: server <port>\n");
        return 1;
    }

    int port = atoi(argv[1]);

    // CREATING SOCKET
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

    // CONSTRUCTED SERVER ADDRESS
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);


    // BIND ADDRESS TO SOCKET
    int did_bind = bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (did_bind < 0) {
        close(sockfd);
        return errno;
    }

    // CLIENT BUFFERS
    int BUF_SIZE = 1024;
    char client_buf[BUF_SIZE];
    struct sockaddr_in clientaddr;
    memset(&clientaddr, 0, sizeof(clientaddr));
    socklen_t clientsize = sizeof(clientaddr);
    bool clientReady = false;

    while (1) {
        ssize_t recv_len = recvfrom(sockfd, client_buf, BUF_SIZE, 0,
                                    (struct sockaddr*)&clientaddr, &clientsize);

        char* client_ip = inet_ntoa(clientaddr.sin_addr);
        int client_port = ntohs(clientaddr.sin_port);

        if (recv_len > 0 || clientReady) {
            clientReady = true;
            write(STDOUT_FILENO, client_buf, recv_len);

            ssize_t read_len = read(STDIN_FILENO, client_buf, BUF_SIZE);
            if (read_len > 0) {
                sendto(sockfd, client_buf, read_len, 0,
                       (struct sockaddr*)&clientaddr, clientsize);
            }
        }
    }

    close(sockfd);
    return 0;
}
