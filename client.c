#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#include "utils.h"


// TODO: read and build packet function


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;
    int next_seq [] = {1, 2, 3, 0};
    int next_ack [] = {0, 1, 2, 3};
    int prev_seq [] = {3, 0, 1, 2};

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // // Connect the sending socket to the proxy's receiving port 5002
    // if ((connect(send_sockfd, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to))) < 0) {
    //     printf("\nConnection Failed \n");
    //     close(send_sockfd);
    //     return 1;
    // }

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // if (listen(listen_sockfd, 10) == -1) {
    //     perror("listen failed");
    //     close(listen_sockfd);
    //     return 1;
    // }

    // while (1) {
    //     client_sockfd = accept(listen_sockfd, (struct sockaddr*)&CLIENT_PORT_TO, sizeof(CLIENT_PORT_TO));
    //     if (client_sockfd == -1) {
    //         perror("accept failed");
    //         continue;
    //     }

        // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    ssize_t bytes_read;
    ssize_t bytes_sent;
    ssize_t bytes_received;

    // set the timeout value
    tv.tv_usec = 100;
    setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(tv)); 
    // FILE *wfp = fopen("output.txt", "wb");

    // BATCH: declare window size, start and end pointer
    
    int valid_size = 0;
    unsigned short start_seq = 0;
    unsigned short last_seq = -1;
    bool empty = false;

    // BATCH: get total file len, file remainder size
    long int len;
    fseek(fp,0,SEEK_END);
    len = ftell(fp);
    long int last_packet_len = len % PAYLOAD_SIZE;
    long int num_packets = len/PAYLOAD_SIZE;
    printf("last packet len: %d, num of packets:%d", last_packet_len,num_packets);

    close(fp);
    fp = fopen(filename, "rb");

    // BATCH: construct and send initial array of packets 
    struct packet packets_in_window[window_size];
    for(int i = 0; i<window_size; i++)
    {   
        num_packets -= 1;
        if(num_packets<0)
            continue;
        bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);

        // TODO: check if packet number < windowsize 
        if(bytes_read<=0)
            break;
        // build packet
        build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, (const char*) buffer);
        packets_in_window[i] = pkt;
        valid_size += 1;
        memset(buffer, 0, sizeof(buffer));

        // send packet
        bytes_sent = sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
        if(bytes_sent>0)
            printf("bytes_send: %d seq:%d\n",bytes_sent,seq_num);

        // increment seq num                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
        seq_num = next_seq[seq_num];
        printf("index:%d", i);

    }
    

    while(bytes_received = recvfrom(listen_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr, &addr_size)>0)
    {
        //printf("received, acknum: %d, start_seq:%d\n", ack_pkt.acknum, start_seq);
       
        
        // if last packet is acked, finish 
        if(ack_pkt.last=='d')
        {
            printf("last packet acked, acknum:%d \n", ack_pkt.acknum);
            break;
        }
        // if start of window ack received, slide window
        if(ack_pkt.acknum==start_seq)
        {
            memset(buffer, 0, sizeof(buffer));
            
            // slide window
            start_seq = next_seq[start_seq];
            for(int i = 0; i<window_size-1;i++)
            {
                packets_in_window[i] = packets_in_window[i+1];
            }
            valid_size -= 1;


            // only create last packet once
            if (num_packets<0 && empty==false)
            {
                bytes_read = fread(buffer,1,last_packet_len,fp);
                buffer[last_packet_len]='\0';
                //printf(buffer);
                //printf("last packet read\nbytes_read:%d\n", bytes_read);
                empty = true;
                build_packet(&pkt, seq_num, ack_num, 't', ack, last_packet_len+1, (const char*) buffer);
                sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                //printf(pkt.payload);
                //printf(pkt.last);
            }
            // keep reading and sending if more is available
            else if (num_packets>0)
            //if(!empty && num_packets>0)
            {
                bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
                build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, (const char*) buffer);
                packets_in_window[window_size-1] = pkt;
                valid_size += 1;
                memset(buffer, 0, sizeof(buffer));

                bytes_sent = sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                printf("bytes_send: %d seq:%d\n",bytes_sent,seq_num);

                // increment seq num                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
                seq_num = next_seq[seq_num];
            }
            num_packets -= 1;

        }
        else
        {
            // send everything in window again
            
            for(int i = 0; i<valid_size; i++)
            {
                struct packet p = packets_in_window[i];
                bytes_sent = sendto(send_sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                //printf("bytes_send: %d seq:%d\n",bytes_sent,p.seqnum);
            }

        }
        //delay(2000);
    }
    
    close(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

