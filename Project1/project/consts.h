#pragma once

#include <arpa/inet.h>
#include <iostream>   // For cout/cerr if needed for debugging
#include <cstring>    // For C++ string functions

// Maximum payload size
#define MAX_PAYLOAD 1012

// Retransmission time
#define TV_DIFF(end, start)                                                    \
    (end.tv_sec * 1000000) - (start.tv_sec * 1000000) + end.tv_usec -          \
        start.tv_usec
#define RTO 1000000
#define MIN(a, b) (a > b ? b : a)
#define MAX(c, d) (c > d ? c : d)

// Window size
#define MIN_WINDOW MAX_PAYLOAD
#define MAX_WINDOW MAX_PAYLOAD * 40
#define DUP_ACKS 3

// States
#define SERVER_AWAIT 0
#define CLIENT_START 1
#define SERVER_START 2
#define CLIENT_AWAIT 3
#define NORMAL 4

// Flags
#define SYN 0b001
#define ACK 0b010

// Diagnostic messages
#define RECV 0
#define SEND 1
#define RTOD 2
#define DUPA 3

// Structs
typedef struct {
    uint16_t seq;
    uint16_t ack;
    uint16_t length;
    uint16_t win;
    uint16_t flags; // LSb 0 SYN, LSb 1 ACK
    uint16_t unused;
    uint8_t payload[0];
} packet;

typedef struct buffer_node {
    struct buffer_node* next;
    packet pkt;
} buffer_node;

// Helpers
static inline void print(const char* txt) {
    std::cerr << txt << std::endl;
}

static inline void print_diag(packet* pkt, int diag) {
    switch (diag) {
    case RECV:
        std::cerr << "RECV";
        break;
    case SEND:
        std::cerr << "SEND";
        break;
    case RTOD:
        std::cerr << "RTOS";
        break;
    case DUPA:
        std::cerr << "DUPS";
        break;
    }

    bool syn = pkt->flags & SYN;
    bool ack = pkt->flags & ACK;
    std::cerr << " " << ntohs(pkt->seq) << " ACK " << ntohs(pkt->ack)
              << " LEN " << ntohs(pkt->length) << " WIN " << ntohs(pkt->win)
              << " FLAGS ";
    if (!syn && !ack) {
        std::cerr << "NONE";
    } else {
        if (syn) {
            std::cerr << "SYN ";
        }
        if (ack) {
            std::cerr << "ACK ";
        }
    }
    std::cerr << std::endl;
}

static inline void print_buf(buffer_node* node) {
    std::cerr << "BUF ";
    while (node != nullptr) {
        std::cerr << htons(node->pkt.seq) << " ";
        node = node->next;
    }
    std::cerr << std::endl;
}
