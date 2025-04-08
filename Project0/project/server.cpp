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

    int port = atoi(argv[1]);
    cout << "Starting server on port " << port << endl;

    //CREATING SOCKET
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    cout << "Socket created with descriptor: " << sockfd << endl;

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

    // CONSTRUCTED SERVER ADDRESS
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // use IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY; // accept all connections
                        // same as inet_addr("0.0.0.0") 
                                 // "Address string to network bytes"

    // Set receiving port
    int PORT = 8080;
    servaddr.sin_port = htons(PORT); // Big endian
    cout << "Server address configured for port " << port << endl;

    // BINDED ADDRESS TO SOCKET
    /* 3. Let operating system know about our config */
    int did_bind = bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    if (did_bind < 0) { // Error if did_bind < 0 :(
        cerr << "Bind failed: " << strerror(errno) << endl;
        close(sockfd);
        return errno;
    }
    cout << "Socket successfully bound to address" << endl;

    // CREATED sockaddr_in AND socklen_t BUFFERS TO STORE CLIENT ADDRESSES
    /* 4. Create buffer to store incoming data */
    int BUF_SIZE = 1024;
    char client_buf[BUF_SIZE];
    struct sockaddr_in clientaddr = {0}; // Same information, but about client
    bool client_connected = false;
    socklen_t clientsize = sizeof(clientaddr);

    cout << "Server started successfully. Waiting for clients..." << endl;

    while (true) {
        // Receive from client
        ssize_t recv_len = recvfrom(sockfd, client_buf, BUF_SIZE, 0, (struct sockaddr*)&clientaddr, &clientsize);
        
        if (recv_len == 0) {
            continue;
        } else if (recv_len < 0) {
            return errno;
        }

        if (!client_connected) {
            client_connected = true;
            char* client_ip = inet_ntoa(clientaddr.sin_addr);
            int client_port = ntohs(clientaddr.sin_port);
            cout << "Client connected from " << client_ip << ":" << client_port<< endl;
        }

        write(1, client_buf, recv_len);

        // Read from stdin
        ssize_t read_len = read(0, client_buf, BUF_SIZE);
        if (read_len > 0) {
            if (client_connected) {
                ssize_t did_send = sendto(sockfd, client_buf, read_len, 0, (struct sockaddr*)&clientaddr, clientsize);
                
                if (did_send < 0) {
                    cerr << "Send error: " << strerror(errno) << endl;
                    return errno;
                } else if (did_send != read_len) {
                    cerr << "Partial send: " << did_send << " of " << read_len << " bytes" << endl;
                }
            }
        } else if (read_len < 0) {
            cerr << "Stdin read error: " << strerror(errno) << endl;
            return errno;
        } else if (read_len == 0) {
            // EOF on stdin
            break;
        }
    }

    cout << "Closing socket and exiting..." << endl;
    close(sockfd);
    return 0;
}
