#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>


// MACROS
#define SERVER_IP "127.0.0.1"
#define LOCAL_HOST "127.0.0.1"
#define SERVER_PORT_TO 5002
#define CLIENT_PORT 6001
#define SERVER_PORT 6002
#define CLIENT_PORT_TO 5001
#define PAYLOAD_SIZE 1024
#define WINDOW_SIZE 4
#define TIMEOUT 0.1
#define MAX_SEQUENCE 1024
#define MAX(a,b) (((a)>=(b))?(a):(b))

// congestion control states
enum state {
  SLOW_START,
  CONGESTION_AVOIDANCE,
  FAST_RETRANSMIT,
  RETRANSMIT_TIMEOUT
};


// ack number next
int next_ack[] = {1,2,3,4,5,0};
int prev_ack = 0;

// window size
// congestion control: start with 1
int window_size = 5;
int server_window_size = 2000;

// Packet Layout
// You may change this if you want to
struct packet {
    unsigned short seqnum;  // 2bytes
    unsigned short acknum; // 2bytes
    char ack; // 1byte
    char last; // 1byte
    unsigned int length; // 4bytes 
    char payload[PAYLOAD_SIZE];  // 1024 byres
    double starttime;
};

// Utility function to build a packet
void build_packet(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char last, char ack,unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->ack = ack;
    pkt->last = last;
    pkt->length = length;
    pkt->starttime = 0;
    memcpy(pkt->payload, payload, length);
}

void change_packet_start_time(struct packet* pkt, double starttime) {
    pkt->starttime = starttime;
}

// Utility function to print a packet
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", (pkt->ack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s\n", pkt->seqnum, pkt->acknum, pkt->last ? " LAST": "", pkt->ack ? " ACK": "");
}

// delay
void delay(int number_of_seconds)
{
    // Converting time into milli_seconds
    int milli_seconds = 1000 * number_of_seconds;
 
    // Storing start time
    clock_t start_time = clock();
 
    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds);
}


// set timer
double setStartTime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec/1000000;
}

// check timeout
int timeout(double startTime){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double currTime = tv.tv_sec + tv.tv_usec/1000000;
    return (currTime-startTime > TIMEOUT);
}


#endif