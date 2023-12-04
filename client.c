#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <fcntl.h>

#include "utils.h"


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
    double startTime;
    // int next_seq [] = {1,2,3,4,5,0};
    enum state STATE = SLOW_START;

    // congestion control window and slow start thresh
    int cwnd = 1;
    int ssthresh = 6;

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

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // // TODO: Read from file, and initiate reliable data transfer to the server
    ssize_t bytes_read;
    ssize_t bytes_sent;
    ssize_t bytes_received;

    // BATCH: get total file len, file remainder size
    long int len;
    fseek(fp,0,SEEK_END);
    len = ftell(fp);
    if(len==0)
    {
        return 0;
    }

    long int last_packet_len = len % PAYLOAD_SIZE;
    long int num_packets = len/PAYLOAD_SIZE + (len % PAYLOAD_SIZE != 0);
    //printf("last packet len: %d, num of packets:%d\n", last_packet_len, num_packets);


    close(fp);
    fp = fopen(filename, "rb");
    
    struct packet packets_window[num_packets];
    while((bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp))> 0){
        if (bytes_read < PAYLOAD_SIZE){
            last = 1;
        }

        build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, (const char*) buffer);
        packets_window[seq_num] = pkt;
        seq_num += 1;
    }   
    close(fp);

    // test if the file in packets_window is correct
    // struct packet current_pkt;
    // FILE *wfp = fopen("output.txt", "wb");
    // for (int i = 0; i < num_packets; i ++){
    //     current_pkt = packets_window[i];
    //     fwrite(current_pkt.payload, 1, current_pkt.length, wfp);
    //     if(current_pkt.last)
    //     {
    //         fclose(wfp);
    //         return EXIT_SUCCESS;
    //     }
    // }

    // BATCH: declare window size, start and end pointer
    int valid_size = 0;
    unsigned short start_seq = 0;
    unsigned short end_seq = cwnd - 1;
    bool empty = false;

/* CONGESTION CONTROL*/
    // set dup ack for fast retransmit
    int dupAckCount = 0;
    int dupAck = -1;
    int last_ack = 0;
    seq_num = 0;
    // printf("current seq: %d, start_window: %d, end_window: %d\n", seq_num, start_seq, end_seq);

    // set count for number of packets received for congestion avoidance
    int pack_recv = 0;

    // send first set of packets
    // num_packets -= 1;
    // bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
    struct packet curr_pkt;
    if (end_seq >= num_packets){
        end_seq = num_packets - 1;
    }

    if (seq_num >= start_seq && seq_num <= end_seq){
        for(int i = seq_num; i <= end_seq; i ++ ){
            curr_pkt = packets_window[i];
            startTime = setStartTime();
            change_packet_start_time(&curr_pkt, startTime);
            bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
            //printf("regular packet %d sent\n", curr_pkt.seqnum);
            packets_window[i] = curr_pkt;
            // printf("this pkt start at %d\n", packets_window[i].starttime);
            // printf("%s", curr_pkt.payload);
        }
        seq_num = end_seq + 1;
    }
    // printf("current seq: %d, start_window: %d, end_window: %d\n", seq_num, start_seq, end_seq);
    
    // set listen socket as non blocking
    fcntl(listen_sockfd, F_SETFL, O_NONBLOCK);
    int timeout_count = 0;
    while(1){
        // if (start_seq == num_packets-1){
        //     delay(4);
        //     close(fp);
        //     close(listen_sockfd);
        //     close(send_sockfd);
        //     return EXIT_SUCCESS;
        // }
        if (bytes_received = recvfrom(listen_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr, &addr_size)>0){
            timeout_count = 0;
            printf("ack num %d received\n", ack_pkt.acknum);
            if (ack_pkt.last){
                //printf("last packet acked, acknum:%d \n", ack_pkt.acknum);
                close(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return EXIT_SUCCESS;
            }

            if(ack_pkt.acknum==last_ack)
            {
                dupAckCount += 1;
                // fast recovery on duplicate ack
                if(STATE==FAST_RETRANSMIT) 
                {
                    cwnd += 1;
                }
            } else {
                dupAckCount = 0;
                // fast recovery on unacked pkt
                if(STATE==FAST_RETRANSMIT)
                {
                    STATE = SLOW_START;
                    cwnd = ssthresh;
                    //cwnd += 1;
                }
            }

            last_ack = ack_pkt.acknum;

            // if three duplicated acks, go to fast retransimit
            if(dupAckCount==3)
            {
                STATE = FAST_RETRANSMIT;
                dupAck = last_ack;
                pack_recv = 0;
            }

            if(STATE==FAST_RETRANSMIT)
            {
                //printf("State: FAST RETRANSMIT\n");
                // retransmit packet with dupAck, use relative positition of curr start seq and returned ack to get that packet
                if (dupAckCount == 3){
                    curr_pkt = packets_window[dupAck];
                    // reset the timer?
                    startTime = setStartTime();
                    change_packet_start_time(&curr_pkt, startTime);
                    bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                    packets_window[dupAck] = curr_pkt;

                    // reset ssthresh and cwnd
                    ssthresh = MAX(cwnd/2, 2);
                    cwnd = ssthresh + 3;
                }

                printf("state: FAST RESTRANSMIT curr cwnd: %d\n", cwnd);

                // change the window range
                start_seq = dupAck;
                end_seq = start_seq + (cwnd - 1);
                if (end_seq >= num_packets){
                    end_seq = num_packets - 1;
                }

                // FAST RECOVERY
                if (seq_num >= start_seq && seq_num <= end_seq){
                    for(int i = seq_num; i <= end_seq; i ++ ){
                        curr_pkt = packets_window[i];
                        startTime = setStartTime();
                        change_packet_start_time(&curr_pkt, startTime);
                        bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                        packets_window[i] = curr_pkt;
                    }
                    seq_num = end_seq + 1;
                }
            } 
            else if(ack_pkt.acknum > start_seq) {
                if(STATE == SLOW_START)
                {
                    cwnd += 1;
                    printf("state: SLOW START curr cwnd: %d\n", cwnd);
                    // TODO: create 2 packets and send + reset timer
                    start_seq = ack_pkt.acknum;
                    end_seq = start_seq + (cwnd - 1);
                    if (end_seq >= num_packets){
                        end_seq = num_packets - 1;
                    }
                    
                    if (seq_num >= start_seq && seq_num <= end_seq){
                        for(int i = seq_num; i <= end_seq; i ++ ){
                            curr_pkt = packets_window[i];
                            startTime = setStartTime();
                            change_packet_start_time(&curr_pkt, startTime);
                            bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                            ("regular packet %d sent\n", curr_pkt.seqnum);
                            packets_window[i] = curr_pkt;
                            // printf("this pkt start at %d\n", packets_window[i].starttime);
                        }
                        seq_num = end_seq + 1;
                    }
                    // end of TODO
                    if(cwnd>ssthresh)
                    {
                        STATE = CONGESTION_AVOIDANCE;
                    }
                }
                else if(STATE == CONGESTION_AVOIDANCE)
                {
                    // cwnd +=1 for every RTT
                    // when to reset pack_recv?
                    pack_recv += 1;
                    if(pack_recv==cwnd)
                    {
                        cwnd += 1;
                        pack_recv = 0;
                        // TODO: create packet and send + reset timer
                    }
                    printf("state: CONGESTION CONTROL curr cwnd: %d pack recv:%d\n", cwnd, pack_recv);

                    // TODO: create packet and send + reset timer
                    start_seq = ack_pkt.acknum;
                    end_seq = start_seq + (cwnd - 1);
                    if (end_seq >= num_packets){
                        end_seq = num_packets - 1;
                    }
                    
                    if (seq_num >= start_seq && seq_num <= end_seq){
                        for(int i = seq_num; i <= end_seq; i ++ ){
                            curr_pkt = packets_window[i];
                            startTime = setStartTime();
                            change_packet_start_time(&curr_pkt, startTime);
                            bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                            printf("regular packet %d sent\n", curr_pkt.seqnum);
                            packets_window[i] = curr_pkt;
                            // printf("this pkt start at %d\n", packets_window[i].starttime); 
                        }
                        seq_num = end_seq + 1;
                    }
                }
            } 
        }
        else if(timeout(packets_window[start_seq].starttime))
        {
            pack_recv = 0;
            timeout_count += 1;
            if (timeout_count >= 3 && seq_num == num_packets){
                //printf("server might be closed\n");
                close(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return EXIT_SUCCESS;
            }
            ssthresh = MAX(cwnd/2, 2);
            cwnd = 1;
            // retransmit lost packet
            curr_pkt = packets_window[start_seq];
            // reset the timer?
            startTime = setStartTime();
            change_packet_start_time(&curr_pkt, startTime);
            bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
            printf("timeout! lost packet %d sent\n", curr_pkt.seqnum);
            packets_window[start_seq] = curr_pkt;
            // change the current window range
            end_seq = start_seq + (cwnd - 1);
            if (end_seq >= num_packets){
                end_seq = num_packets - 1;
            }
            // if (seq_num >= start_seq && seq_num <= end_seq){
            //     for(int i = seq_num; i <= end_seq; i ++ ){
            //         curr_pkt = packets_window[i];
            //         startTime = setStartTime();
            //         change_packet_start_time(&pkt, startTime);
            //         bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
            //         packets_window[start_seq] = curr_pkt;
            //     }
            //     seq_num = end_seq + 1;
            // }
            STATE = SLOW_START;
        }
    }
}

/* GO BACK N*/
/*

    // BATCH: construct and send initial array of packets 
    struct packet packets_in_window[window_size];
    for(int i = 0; i<window_size; i++)
    {   
        num_packets -= 1;
        // set timer for first packet
        if (i==0)
        {
            //tv.tv_sec = TIMEOUT;
            //setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(tv)); 
            startTime = setStartTime();
            //printf("startTime:%d\n", startTime);
        }
        bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);

        // if current packet is the last one, send packet with last flag
        // if(num_packets==-1)
        // {
        //     build_packet(&pkt, seq_num, ack_num, 't', ack, last_packet_len+1, (const char*) buffer);
        //     packets_in_window[i] = pkt;
        //     valid_size += 1;
        //     bytes_sent = sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
        //     if(bytes_sent>0)
        //         printf("bytes_send: %d seq:%d\n",bytes_sent,seq_num);
        //     break;
        // }

        if(bytes_read<=0)
            break;

        // build packet
        build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, (const char*) buffer);
        packets_in_window[i] = pkt;
        valid_size += 1;
        //printf("valid size:%d", valid_size);
        memset(buffer, 0, sizeof(buffer));

        // send packet
        bytes_sent = sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
        if(bytes_sent>0)
            printf("bytes_send: %d seq:%d\n",bytes_sent,seq_num);

        // increment seq num                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
        seq_num = next_seq[seq_num];
        //printf("index:%d\n", i);
    }
    


    while (1){


        // if no timeout, check ack number
        if (bytes_received = recvfrom(listen_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr, &addr_size)>0){

            //printf("receiving something");
            // if last packet is acked, finish 
            // can change big while loop to while(num_packets>=0)
            if(ack_pkt.last=='d')
            {
                //printf("last packet acked, acknum:%d \n", ack_pkt.acknum);
                close(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return EXIT_SUCCESS;
            }
            // if ack number is expected, move window, reset timer
            // if wrong ack number, do nothing
            if(ack_pkt.acknum==start_seq)
            {
                memset(buffer, 0, sizeof(buffer));
            
                // slide window
                start_seq = next_seq[start_seq];
                for(int i = 0; i<valid_size-1;i++)
                {
                    packets_in_window[i] = packets_in_window[i+1];
                }
                valid_size -= 1;
    
                // only create last packet once
                // TODO: num_packets==0?

                // keep reading and sending if more is available
                //printf("curr seq:%d", seq_num);
                if (num_packets>0)
                {
                    bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
                    //printf(buffer);
                    build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, (const char*) buffer);

                    packets_in_window[window_size-1] = pkt;
                    valid_size += 1;
            
                    bytes_sent = sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                    //printf("bytes_send: %d seq:%d\n",bytes_sent,seq_num);

                    // increment seq num                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
                    seq_num = next_seq[seq_num];
                } else if (num_packets==0) {
                    bytes_read = fread(buffer,1,PAYLOAD_SIZE,fp);
                    buffer[bytes_read]='\0';
                    //printf(buffer);
                    printf("last packet read, seq num: %d\nbytes_read:%d\n", seq_num, bytes_read);
                    empty = true;
                    
                    build_packet(&pkt, seq_num, ack_num, 't', ack, bytes_read+1, (const char*) buffer);
                    packets_in_window[window_size-1] = pkt;
                    valid_size += 1;
                    sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                    //tv.tv_sec = TIMEOUT;
                    //printf(pkt.payload);
                    //printf(pkt.last);
                }
                num_packets -= 1;
                // reset timer
                startTime = setStartTime();

                //printf("right ack: %d, next starting seq:%d, validsize:%d\n", ack_pkt.acknum, start_seq, valid_size);
            }
            else{
                //printf("wrong ack received: %d, current starting seq: %d\n", ack_pkt.acknum, start_seq);
            }
        }
        // if timeout, resend
        else if(timeout(startTime))
        {
            //printf("timeout, validsize:%d\n", valid_size);
            for(int i = 0; i<valid_size; i++)
            {
                struct packet p = packets_in_window[i];
                //printf(pkt.payload);
    
                //delay(1);
                bytes_sent = sendto(send_sockfd, (void *) &p, sizeof(p), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                startTime = setStartTime();
                //printf("timeout!!bytes_send: %d seq:%d windowsize:%d validsize:%d\n",bytes_sent,p.seqnum, window_size,valid_size);
            }
        }
        //delay(1);
        
    }
}
*/