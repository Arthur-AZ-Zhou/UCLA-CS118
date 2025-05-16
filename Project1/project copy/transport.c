#include "consts.h"
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/*
 * the following variables are only informational, 
 * not necessarily required for a valid implementation.
 * feel free to edit/remove.
 */
int state = 0;           // Current state for handshake
int our_send_window = 0; // Total number of bytes in our send buf
// int their_receiving_window = MIN_WINDOW;   // Receiver window size
// int our_max_receiving_window = MIN_WINDOW; // Our max receiving window
int their_receiving_window = MIN_WINDOW;   // Receiver window size
int our_max_receiving_window = MAX_WINDOW; // Our max receiving window
int our_recv_window = 0;                   // Bytes in our recv buf
int dup_acks = 0;        // Duplicate acknowledgements received
uint32_t ack = 0;        // Acknowledgement number
uint32_t seq = 0;        // Sequence number
uint32_t last_ack = 0;   // Last ACK number to keep track of duplicate ACKs
bool pure_ack = false;  // Require ACK to be sent out
packet* base_pkt = NULL; // Lowest outstanding packet to be sent out

int total_bytes = 0; // Total bytes received

buffer_node* recv_buf_head =
    NULL; // Linked list storing out of order received packets
buffer_node* send_buf_head =
    NULL; // Linked list storing packets that were sent but not acknowledged
buffer_node* send_buf_tail =
    NULL;

ssize_t (*input)(uint8_t*, size_t); // Get data from layer
void (*output)(uint8_t*, size_t);   // Output data from layer

struct timeval start; // Last packet sent at this time
struct timeval now;   // Temp for current time

// Get data from standard input / make handshake packets
packet* get_data() {
    switch (state) {
    case SERVER_AWAIT:
        return NULL;

    case CLIENT_AWAIT:
        return NULL;

    case CLIENT_START: {
        state = CLIENT_AWAIT;

        uint8_t read_payload[MAX_PAYLOAD];
        int bytes_read = input(read_payload, MAX_PAYLOAD);
        fprintf(stderr, "%d bytes read\n", bytes_read);

        // Create packet
        packet *p = calloc(1, sizeof(packet) + bytes_read);
        p->seq = htons(seq);
        p->ack = htons(ack);
        p->length = htons(bytes_read);
        p->win = htons(MAX_WINDOW);
        p->flags = SYN;
        p->unused = 0;
        memcpy(p->payload, read_payload, bytes_read);

        // Put packet in sending buffer
        buffer_node *b = malloc(sizeof(buffer_node) + bytes_read);
        memset(b, 0, sizeof(buffer_node) + bytes_read);
        b->next = NULL;
        memcpy(&b->pkt, p, sizeof(packet));
        memcpy(b->pkt.payload, p->payload, bytes_read);
        send_buf_head = b;
        send_buf_tail = b;

        base_pkt = &send_buf_head->pkt;
        our_send_window += bytes_read;
        seq += 1;
        
        return p;
    }

    case SERVER_START: {
        state = SERVER_AWAIT;

        uint8_t read_payload[MAX_PAYLOAD];
        int bytes_read = input(read_payload, MAX_PAYLOAD);
        fprintf(stderr, "%d bytes read\n", bytes_read);

        // Create packet
        packet *p = calloc(1, sizeof(packet) + bytes_read);
        p->seq = htons(seq);
        p->ack = htons(ack);
        p->length = htons(bytes_read);
        p->win = htons(MAX_WINDOW);
        p->flags = SYN | ACK;
        p->unused = 0;
        memcpy(p->payload, read_payload, bytes_read);

        // Put packet in sending buffer
        buffer_node *b = malloc(sizeof(buffer_node) + bytes_read);
        memset(b, 0, sizeof(buffer_node) + bytes_read);
        b->next = NULL;
        memcpy(&b->pkt, p, sizeof(packet));
        memcpy(b->pkt.payload, p->payload, bytes_read);
        send_buf_head = b;
        send_buf_tail = b;

        base_pkt = &send_buf_head->pkt;
        our_send_window += bytes_read;
        seq += 1;
        return p;
    }

    default: {
        // fprintf(stderr, "Their receiving window: %d\n", their_receiving_window);
        // fprintf(stderr, "Our sending window: %d\n", our_send_window);
        if (our_send_window + MAX_PAYLOAD <= their_receiving_window) {
            uint8_t read_payload[MAX_PAYLOAD];
            int bytes_read = input(read_payload, MAX_PAYLOAD);
            fprintf(stderr, "%d bytes read\n", bytes_read);

            // int number_of_bytes = their_receiving_window - our_send_window;
            // if (number_of_bytes > MAX_PAYLOAD) number_of_bytes = MAX_PAYLOAD;
            // if (number_of_bytes <= 0) return NULL;

            // uint8_t read_payload[number_of_bytes];
            // int bytes_read = input(read_payload, number_of_bytes);
            // fprintf(stderr, "%d bytes read\n", bytes_read);

            if (bytes_read == 0) {
                pure_ack = true;
                return NULL;
            }

            if (bytes_read < 0) return NULL;

            // Create packet
            packet *p = calloc(1, sizeof(packet) + bytes_read);
            p->seq = htons(seq);
            p->ack = htons(ack);
            p->length = htons(bytes_read);
            p->win = htons(MAX_WINDOW);
            p->flags = ACK;
            p->unused = 0;
            memcpy(p->payload, read_payload, bytes_read);

            buffer_node *b = malloc(sizeof(buffer_node) + bytes_read);
            memset(b, 0, sizeof(buffer_node) + bytes_read);
            b->next = NULL;
            memcpy(&b->pkt, p, sizeof(packet));
            memcpy(b->pkt.payload, p->payload, bytes_read);

            if (send_buf_head == NULL) {
                send_buf_head = b;
                send_buf_tail = b;
            } else {
                send_buf_tail->next = b;
                send_buf_tail = b;
            }

            base_pkt = &send_buf_head->pkt;
            our_send_window += bytes_read;
            seq += 1;
            return p;
        }
        return NULL;
    }
    }
}

// Process data received from socket
void recv_data(packet* pkt) {
    uint16_t payload_len = ntohs(pkt->length);
    their_receiving_window = ntohs(pkt->win);

    switch (state) {
    case CLIENT_START:
        return;
    case SERVER_START: {
        return;
    }
    case SERVER_AWAIT: {
        if (pkt->flags & SYN) {
            ack = ntohs(pkt->seq) + 1;
            state = SERVER_START;

            if (ntohs(pkt->length) > 0) {
                output(pkt->payload, payload_len);
                fprintf(stderr, "Received sequence number %u\n", ntohs(pkt->seq));
                fprintf(stderr, "%hu bytes outputted\n", payload_len + total_bytes);
                total_bytes += payload_len;
            }

            // Not an ACK, so no need to modify sending buffer
            // No need to add to the receiving buffer

        } else if (pkt->flags & ACK) {
            ack = ntohs(pkt->seq) + 1;
            state = NORMAL;

            if (ntohs(pkt->length) > 0) {
                // pure_ack = true;
                output(pkt->payload, payload_len);
                fprintf(stderr, "Received sequence number %u\n", ntohs(pkt->seq));
                fprintf(stderr, "%hu bytes outputted\n", payload_len + total_bytes);
                total_bytes += payload_len;
            }

            // Remove ACKed packets from the sending buffer and flow control window
            while (send_buf_head && ntohs(send_buf_head->pkt.seq) < ntohs(pkt->ack)) {
                our_send_window -= ntohs(send_buf_head->pkt.length);
                buffer_node *temp = send_buf_head;
                send_buf_head = send_buf_head->next;
                free(temp);
                base_pkt = send_buf_head ? &send_buf_head->pkt : NULL;
            }

            // No need to add to the receiving buffer
        }
        return;
    }

    case CLIENT_AWAIT: {
        if (pkt->flags & (SYN | ACK)) {
            ack = ntohs(pkt->seq) + 1;
            state = NORMAL;
            
            if (ntohs(pkt->length) > 0) {
                // pure_ack = true;
                output(pkt->payload, payload_len);
                fprintf(stderr, "Received sequence number %u\n", ntohs(pkt->seq));
                fprintf(stderr, "%hu bytes outputted\n", payload_len + total_bytes);
                total_bytes += payload_len;
            }

            // Remove ACKed packets from the sending buffer and flow control window
            while (send_buf_head && ntohs(send_buf_head->pkt.seq) < ntohs(pkt->ack)) {
                our_send_window -= ntohs(send_buf_head->pkt.length);
                buffer_node *temp = send_buf_head;
                send_buf_head = send_buf_head->next;
                free(temp);
                base_pkt = send_buf_head ? &send_buf_head->pkt : NULL;
            }

            // No need to add to the receiving buffer
        }
        return;
    }

    default: {
        if (ntohs(pkt->seq) < ack) {
            // Duplicate packet received
            fprintf(stderr, "Duplicate packet received\n");
        } else if (ntohs(pkt->seq) == ack) {
            output(pkt->payload, payload_len);
            fprintf(stderr, "Received sequence number %u\n", ntohs(pkt->seq));
            fprintf(stderr, "%hu bytes outputted\n", payload_len + total_bytes);
            total_bytes += payload_len;
            // Update ACK number
            ack++;
            while (recv_buf_head) {
                if (ntohs(recv_buf_head->pkt.seq) != ack) break;
                // Output the data
                output(recv_buf_head->pkt.payload, ntohs(recv_buf_head->pkt.length));
                fprintf(stderr, "%hu bytes outputted\n", ntohs(recv_buf_head->pkt.length) + total_bytes);
                total_bytes += ntohs(recv_buf_head->pkt.length);
                our_recv_window -= ntohs(recv_buf_head->pkt.length);
                buffer_node *temp = recv_buf_head;
                recv_buf_head = recv_buf_head->next;
                free(temp);
                ack++;
            }

        } else if (ntohs(pkt->length) > 0) {
            fprintf(stderr, "Out-of-order packet received\n");
            // Add to receiving buffer
            buffer_node *b = malloc(sizeof(buffer_node) + payload_len);
            memset(b, 0, sizeof(buffer_node) + payload_len);
            memcpy(&b->pkt, pkt, sizeof(packet));
            memcpy(b->pkt.payload, pkt->payload, payload_len);
            b->next = NULL;

            buffer_node *prev = NULL;
            buffer_node *curr = recv_buf_head;
            while (curr && ntohs(curr->pkt.seq) < ntohs(pkt->seq)) {
                prev = curr;
                curr = curr->next;
            }
            if (curr && ntohs(curr->pkt.seq) == ntohs(pkt->seq)) {
                free(b);  // Already received this packet
                break;
            }
            if (!prev) {
                b->next = recv_buf_head;
                recv_buf_head = b;
            } else {
                b->next = curr;
                prev->next = b;
            }
            our_recv_window += payload_len;
        }

        if (pkt->flags & ACK) {
            uint16_t curr_ack = ntohs(pkt->ack);
            if (curr_ack == last_ack) dup_acks++;
            else if (curr_ack > last_ack) {
                last_ack = curr_ack;
                dup_acks = 0;
            }

            while (send_buf_head && ntohs(send_buf_head->pkt.seq) < curr_ack) {
                our_send_window -= ntohs(send_buf_head->pkt.length);
                fprintf(stderr, "Our send window after subtracting: %d\n", our_send_window);
                buffer_node *temp = send_buf_head;
                send_buf_head = send_buf_head->next;
                free(temp);
                base_pkt = send_buf_head ? &send_buf_head->pkt : NULL;
            }

            gettimeofday(&start, NULL);
        }
        return;
    }
    }
}


// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int initial_state,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t)) {

    // Set initial state (whether client or server)
    state = initial_state;

    // Set input and output function pointers
    input = input_p;
    output = output_p;

    // Set socket for nonblocking
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &(int) {1}, sizeof(int));

    // Set initial sequence number
    uint32_t r;
    int rfd = open("/dev/urandom", 'r');
    read(rfd, &r, sizeof(uint32_t));
    close(rfd);
    srand(r);
    seq = (rand() % 10) * 100 + 100;

    // Setting timers
    gettimeofday(&now, NULL);
    gettimeofday(&start, NULL);

    // Create buffer for incoming data
    char buffer[sizeof(packet) + MAX_PAYLOAD] = {0};
    packet* pkt = (packet*) &buffer;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    // Start listen loop
    while (true) {
        memset(buffer, 0, sizeof(packet) + MAX_PAYLOAD);
        // Get data from socket
        int bytes_recvd = recvfrom(sockfd, &buffer, sizeof(buffer), 0,
                                   (struct sockaddr*) addr, &addr_size);
        // If data, process it
        if (bytes_recvd > 0) {
            print_diag(pkt, RECV);
            recv_data(pkt);
            print_buf(recv_buf_head);
        }

        packet* tosend = get_data();
        // Data available to send

        if (tosend != NULL) {
            sendto(sockfd, tosend, sizeof(packet) + ntohs(tosend->length), 0, (struct sockaddr*) addr, addr_size);
            print_diag(tosend, SEND);
            print_buf(send_buf_head);
            free(tosend);
        }
        // Received a packet and must send an ACK
        else if (pure_ack) {
            pure_ack = false;

            packet* ack_pkt = calloc(1, sizeof(packet));
            memset(ack_pkt, 0, sizeof(packet));
            ack_pkt->seq = htons(0);
            ack_pkt->ack = htons(ack);
            ack_pkt->length = htons(0);
            ack_pkt->win = htons(our_max_receiving_window);
            ack_pkt->flags = ACK;
            ack_pkt->unused = htons(0);

            sendto(sockfd, ack_pkt, sizeof(packet), 0, (struct sockaddr*) addr, addr_size);
            print_diag(ack_pkt, SEND);
            print_buf(send_buf_head);
            free(ack_pkt);
        }

        // Check if timer went off
        gettimeofday(&now, NULL);
        if (TV_DIFF(now, start) >= RTO && base_pkt != NULL) {
            sendto(sockfd, base_pkt, sizeof(packet) + ntohs(base_pkt->length), 0, (struct sockaddr*)addr, addr_size);
            gettimeofday(&start, NULL);
        }
        // Duplicate ACKS detected
        else if (dup_acks == DUP_ACKS && base_pkt != NULL) {
            sendto(sockfd, base_pkt, sizeof(packet) + ntohs(base_pkt->length), 0, (struct sockaddr*)addr, addr_size);
            dup_acks = 0;
        }
        // No data to send, so restart timer
        // TODO: Somewhere make base packet NULL
        else if (base_pkt == NULL) {
            gettimeofday(&start, NULL);
        }
    }
}