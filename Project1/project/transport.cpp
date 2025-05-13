#include "consts.h"
#include <arpa/inet.h>
#include <iostream>  
#include <cstdlib>
#include <cstring>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;

/*
 * the following variables are only informational, 
 * not necessarily required for a valid implementation.
 * feel free to edit/remove.
 */
//DEFAULT STATE IS -1
int state = 0;           // Current state for handshake
int our_send_window = 0; // Total number of bytes in our send buf
int their_receiving_window = MIN_WINDOW;   // Receiver window size
int our_max_receiving_window = MIN_WINDOW; // Our max receiving window
int our_recv_window = 0;                   // Bytes in our recv buf
int dup_acks = 0;        // Duplicate acknowledgements received
uint32_t ack = 0;        // Acknowledgement number
uint32_t seq = 0;        // Sequence number, GONNA BE MADE TO BE RANDOM AT START
uint32_t last_ack = 0;   // Last ACK number to keep track of duplicate ACKs
bool pure_ack = false;   // Require ACK to be sent out
packet* base_pkt = nullptr; // Lowest outstanding packet to be sent out

buffer_node* recv_buf = nullptr; // Linked list storing out of order received packets
buffer_node* send_buf = nullptr; // Linked list storing packets that were sent but not acknowledged

ssize_t (*input)(uint8_t*, size_t); // Get data from layer
void (*output)(uint8_t*, size_t);   // Output data from layer

struct timeval start; // Last packet sent at this time
struct timeval now;   // Temp for current time

//MY ADDED VARIABLES & HELPER FUNCTIONS======================================================================
buffer_node* send_bufTail = nullptr; // tail of linked list
stack<buffer_node*> buffNodeStack;
bool handshakeCompleted = false; 
int sockFDrcvd = 0;
struct sockaddr_in* socketAddr;

//NOTE: Socket file descriptor (sockfd) for UDP: int sockfd = socket(AF_INET, SOCK_DGRAM, 0); 

packet* makePureAck() {  //NO PAYLOAD
    packet* p = new packet();

    memset(p, 0, sizeof(packet));
    p->seq = htons(0); //pure ACKS SEQ is 0
    p->ack = htons(ack);
    p->length = htons(0);
    p->win = htons(MAX_WINDOW);
    p->flags = 0b0010; //ACK LSB is at 1
    p->unused = htons(0);
    //NO PAYLOAD

    return p;
}

packet* makeDataPacket(uint8_t* data, int len) { //YES PAYLOAD
    packet* p = static_cast<packet*> (calloc(1, sizeof(packet) + len));

    if (p == nullptr) {
        cerr << "ALLOCATION FAILURE FOR MAKEDATAPACKET" << endl;
        return nullptr;
    }

    p->seq = htons(seq);
    p->ack = htons(ack);
    p->length = htons(static_cast<uint16_t> (len));
    p->win = htons(MAX_WINDOW);
    p->flags = 0; //DEFAULT PACKET AFTER HANDSHAKE
    p->unused = htons(0);
    memcpy(p->payload, data, len);

    return p;
}

void sendPureAck(int sockFD, sockaddr_in* socketAddr) { //NO PAYLOAD
    packet* p = makePureAck();

    sendto(sockFD, p, sizeof(packet), 0, (sockaddr*) socketAddr, sizeof(sockaddr_in));
    cerr << "sockFD: " << sockFD << ", packet SEQ and ACK: " << p->seq << " & " << p->ack << endl;

    delete p; //FREE UP MEMORY IMPORTANT
}

void sendDataPacket(uint8_t* data, int len, int sockFD, sockaddr_in* socketAddr) { //YES PPAYLOAD
    packet* p = makeDataPacket(data, len);

    sendto(sockFD, p, sizeof(packet), 0, (sockaddr*) socketAddr, sizeof(sockaddr_in));
    cerr << "sockFD: " << sockFD << ", packet SEQ and ACK and LENGTH: " << p->seq << " & " << p->ack << p->length << endl;

    delete p;
}

void printBytesBinary(uint16_t value) { //unlike c we have bitset
    uint8_t high = (value >> 8) & 0xFF; 
    uint8_t low  = value & 0xFF; 

    cerr << "first 8 bits: " << bitset<8>(high) << endl;
    cerr << "second 8 bites: " << bitset<8>(low) << endl;
}

void printDeque(deque<buffer_node*> deq) {
    for (auto it = deq.begin(); it != deq.end(); ++it) {
        cerr << "SEQ: " << ntohs((*it)->pkt.seq);
    }
    cerr << endl;
}
//END OF MY ADDED VARIABLES & HELPER FUNCTIONS======================================================================

// Get data from standard input / make handshake packets
packet* get_data() {
    switch (state) {
        case SERVER_AWAIT: //return nullptr
            cerr << "SERVER IS AWAITING" << endl;
            // cerr << "trigger1" << endl;
            return nullptr;

        case CLIENT_AWAIT: //also return nullptr
            cerr << "CLIENT IS AWAITING" << endl;
            // cerr << "trigger2" << endl;
            return nullptr;

        case CLIENT_START: //ones in parenthesis are the valid ones
            if (handshakeCompleted) { //client sends server syn, then server sends back syn ack, (then client sends back ack)
                cerr << "FINAL HANDSHAKE ACK SENT BY CLIENT, ACK NUMBER: " << ack << ", SEQ NUMBER: " << seq << "\nSTARTING DEFAULT TRANSMISSION" << endl;
                state = -1; //DEFAULT state is -1

                return makePureAck(); // NO PAYLOAD NEEDED ACCORDING TO SPEC
            } else { //(client sends server syn), then server sends back syn ack, then client sends back ack
                state = CLIENT_AWAIT;
                packet* p = makePureAck();

                p->seq = htons(seq);
                p->flags = 0b0001;
                cerr << "FIRST PART OF HANDSHAKE: " << p->seq << ", " << p->ack << ", " << p->flags << endl;

                handshakeCompleted = true;
                return p;
            }

        case SERVER_START: //client sends server syn, (then server sends back syn ack), then client sends back ack
            if (handshakeCompleted) {
                return nullptr;
            } else {
                cerr << "SERVER SENDING SYN ACK" << endl;
                packet* p = makePureAck();

                p->seq = htons(seq);
                p->flags = 0b011; //turn BOTH SYN AND ACK ARE ON

                handshakeCompleted = true;
                return p;  
            }

        default: 
            if (our_send_window < their_receiving_window) {
                // int readPayload[MAX_PAYLOAD];
                uint8_t readPayload[MAX_PAYLOAD]; //PAYLOAD IS in uint_8
                int bytes_read = input(readPayload, MAX_PAYLOAD);

                if (0 < bytes_read) {
                    cerr << "RECEIVING WINDOW SIZE: " << their_receiving_window << ", SEND WINDOW SIZE: " << our_send_window << endl;
                    
                    for (int i = 0; i < MAX_PAYLOAD; i++) {
                        cerr << readPayload[i] << endl;
                    }

                    return makeDataPacket(readPayload, bytes_read);
                }

                cerr << "!!!!!!!!!!!!!!!!0 BYTES READ" << endl;
                return nullptr;
            } else {
                return nullptr;
            }
    }
}

// Process data received from socket
void recv_data(packet* p) { 
    //NOTE: everything EXCEPT for flags has to be converted to little endian
    switch (state) {
        case CLIENT_START:
            return;

        case SERVER_START: //client sends server syn, then server sends back syn ack, (then client sends back ack)
            cerr << "trigger server start" << endl;

            if (p->flags & 0b0010) { //ACK is marked with LSB in position 1 is 1
                state = -1;
                // cerr << p->flags << endl
                printBytesBinary(p->flags);
                cerr << "beginning transmission" << endl;

                if (0 < ntohs(p->length)) {
                    output(p->payload, ntohs(p->length));
                    // cout << p->payload << endl;
                    ack = ntohs(p->seq) + 1;
                } 
                // else {
                //     ack = ntohs(p->seq); //OTHERWISE DO NOT INCREMENT
                // }
            }
            
            return;

        case SERVER_AWAIT: //(client sends server syn), then server sends back syn ack, then client sends back ack
            //cer << "SERVER AWAITINGF" << endl;

            if (p->flags & 0b0001) { //check if SYN is on
                state = SERVER_START;
                
                if (0 < ntohs(p->length)) {
                    output(p->payload, ntohs(p->length));
                    // ack = ntohs(p->seq) + 1;
                }

                ack = ntohs(p->seq) + 1;
                cerr << "ack at SERVER_AWAIT: " << ack << endl;
            }

            return;

        case CLIENT_AWAIT: //client sends server syn, (then server sends back syn ack), then client sends back ack
            if (p->flags & 0b0011) {
                state = CLIENT_START;

                if (0 < ntohs(p->length)) {
                    output(p->payload, ntohs(p->length));
                    // ack = ntohs(p->seq) + 1;
                }

                ack = ntohs(p->seq) + 1;
            }

            return;

        default: //HARD PART!!!!!!!!!!!!!!!!!!!!!!!==========================
        /*
            static deque<buffer_node*> recvDeque;
            static deque<buffer_node*> sendDeque;

            if (p->flags & 0b0010) {
                if (p->flags == last_ack) {
                    dup_acks++;
                } else { // Reset current ACK
                    last_ack = p->flags;
                    dup_acks = 1;
                }
                
                // 3 duplicate ACKS detected, fast retransmit
                if (dup_acks == DUP_ACKS && !sendDeque.empty()) {
                    packet* base_pkt_ptr = &(sendDeque.front()->pkt);
                    sendto(sockFDrcvd, base_pkt_ptr, sizeof(packet) + ntohs(base_pkt_ptr->length), 0, (sockaddr *) socketAddr, sizeof(sockaddr_in));
                    print_diag(base_pkt_ptr, RTOD);
                    dup_acks = 1;
                }

                // Remove any ACKed packets from the sending buffer and flow control window
                while (!sendDeque.empty() && ntohs(sendDeque.front()->pkt.seq) < p->ack) {
                    buffer_node *old = sendDeque.front();
                    our_send_window -= ntohs(old->pkt.length); // Update sender FCW
                    sendDeque.pop_front();
                    free(old);
                }

                if (sendDeque.empty()) { //reset base packet if deque empty
                    base_pkt = NULL;
                } else {
                    base_pkt = &(sendDeque.front()->pkt); // point base packet to oldest packet
                    gettimeofday(&start, NULL);            // Restart the timer for the new oldest packet
                }
                
                cerr << "received ACK: " << ntohs(p->ack) << " in deque" << endl;
                printDeque(sendDeque);
            }
            */

            

            return;
    }
}

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int initial_state, ssize_t (*input_p)(uint8_t*, size_t), void (*output_p)(uint8_t*, size_t)) {

    // Set initial state (whether client or server)
    state = initial_state;

    // Set input and output function pointers
    input = input_p;
    output = output_p;

    // Set socket for nonblocking
    int flags = fcntl(sockfd, F_GETFL);
    int opt_val = 1;
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));

    // Set initial sequence number
    uint32_t r;
    int rfd = open("/dev/urandom", 'r');
    read(rfd, &r, sizeof(uint32_t));
    close(rfd);
    srand(r);
    seq = (rand() % 10) * 100 + 100;

    // Setting timers
    gettimeofday(&now, nullptr);
    gettimeofday(&start, nullptr);

    // Create buffer for incoming data
    char buffer[sizeof(packet) + MAX_PAYLOAD] = {0};
    packet* pkt = (packet*) &buffer;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    // Start listen loop
    while (true) {
        memset(buffer, 0, sizeof(packet) + MAX_PAYLOAD);
        // Get data from socket
        int bytes_recvd = recvfrom(sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr*) addr, &addr_size);
        // If data, process it
        if (bytes_recvd > 0) {
            print_diag(pkt, RECV);
            recv_data(pkt);
        }

        packet* tosend = get_data();

        if (tosend != nullptr) { // Data available to send
            // Code to send packet
        } else if (pure_ack) { // Received a packet and must send an ACK
            // Code to send ACK
        }

        // Check if timer went off
        gettimeofday(&now, nullptr);
        if (TV_DIFF(now, start) >= RTO && base_pkt != nullptr) {
            // Handle RTO
        } else if (dup_acks == DUP_ACKS && base_pkt != nullptr) { // Duplicate ACKS detected
            // Handle duplicate ACKs
        } else if (base_pkt == nullptr) { // No data to send, so restart timer
            gettimeofday(&start, nullptr);
        }
    }
}
