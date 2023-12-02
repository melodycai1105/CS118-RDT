#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

int main() {
    
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int receive_base = -1;
    int ooo_seq_num = -1;
    int recv_len;
    struct packet ack_pkt;
    // ack_pct ack num default to -1
    ack_pkt.acknum = -1;
    int received[server_window_size];
    struct packet server_packets_window[server_window_size];

    for (int i = 0; i < server_window_size; i ++){
        received[i] = 0;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed: listen");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Selective Repeat
    // Open the target file for writing (always write to output.txt)
    struct packet cur_packet;
    FILE *fp = fopen("output.txt", "wb");
    while (1) {
        ssize_t bytes_read;
        struct packet pkt;

        if (bytes_read = recvfrom(listen_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr, &addr_size) > 0) {
            //printRecv(&pkt);
            //printf("last char: %c", pkt.last);
            // if seq number is prev_ack's next or starting new file
            if(pkt.seqnum == prev_ack)
            {
                printf("good seq number: %d\n", pkt.seqnum);
                receive_base = prev_ack;
                received[receive_base] = 1;
                server_packets_window[receive_base] = pkt;
                while (receive_base < server_window_size && received[receive_base] == 1){
                    cur_packet = server_packets_window[receive_base];
                    if(cur_packet.last)
                    {
                        fwrite(cur_packet.payload, 1, cur_packet.length, fp);
                        ack_pkt.last= 1;
                        ack_pkt.acknum = receive_base + 1;
                        prev_ack = receive_base + 1;
                        sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));
                        printf("last ack packet with ack num %d\n", ack_pkt.acknum);
                        fclose(fp);
                        close(listen_sockfd);
                        close(send_sockfd);
                        return EXIT_SUCCESS;
                    }
                    fwrite(cur_packet.payload, 1, cur_packet.length, fp);
                    // printf("content write: %d\n", cur_packet.length);
                    receive_base += 1;
                    printf("ack num: %d\n", receive_base);
                }
                ack_pkt.acknum = receive_base;
                prev_ack = receive_base;
                sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));
            }
            // congestion control: not in order, buffer
            else{
                // printf("wrong seq num: %d\n", pkt.seqnum);
                ooo_seq_num = pkt.seqnum;
                received[ooo_seq_num] = 1;
                server_packets_window[ooo_seq_num] = pkt;
                // send previous ack number back 
                //sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));

                // send most recent seq as ack
                //printf("bad seq number: %d,expect: %d\n", pkt.seqnum,next_ack[prev_ack]);
                ack_pkt.acknum = prev_ack;
                printf("ack num: %d with wrong seq num: %d\n", prev_ack, pkt.seqnum);
                sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));
            }
        // }
        //delay(1);
        //buffer[bytes_read] = '\0';
        //close(client_socket);
        }
    }   
    // fwrite('\0', 1, 1, fp);
    // fclose(fp);
    // close(listen_sockfd);
    // close(send_sockfd);
    // return 0;
    
    // GO-Back-N
    // // Open the target file for writing (always write to output.txt)
    // FILE *fp = fopen("output.txt", "wb");
    // while (1) {
        
    //     ssize_t bytes_read;
    //     struct packet pkt;

    //     if (bytes_read = recvfrom(listen_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr, &addr_size) > 0) {
            
    //         //printRecv(&pkt);
    //         //printf("last char: %c", pkt.last);

    //         // if seq number is prev_ack's next or starting new file
    //         if(prev_ack==-1 && pkt.seqnum==0 || pkt.seqnum == next_ack[prev_ack])
    //         {
    //             printf("good seq number: %d\n", pkt.seqnum);
    //             //printf("last char: %c", pkt.last);
    //             // memcpy(payload, pkt.payload, length);

    //             //printf("bytes write: %d\n", bytes_write);
    //             //printf(pkt.payload);
                
    //             // send ack number back 
    //             if(pkt.last=='t')
    //             {
    //                 fwrite(pkt.payload, 1, pkt.length-1, fp);
    //                 ack_pkt.last='d';
    //                 ack_pkt.acknum = pkt.seqnum;
    //                 prev_ack = ack_pkt.acknum;
    //                 sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));
    //                 //printf("last packet\n");
    //                 fclose(fp);
    //                 close(listen_sockfd);
    //                 close(send_sockfd);
    //                 return EXIT_SUCCESS;
    //                 //printf(pkt.payload);
    //             }
    //             fwrite(pkt.payload, 1, pkt.length, fp);
                
    //             ack_pkt.acknum = pkt.seqnum;
    //             prev_ack = ack_pkt.acknum;
    //             sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));
    //         }
    //         // congestion control: not in order, buffer
    //         else{
    //             // send previous ack number back 
    //             //sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));

    //             // send most recent seq as ack
    //             //printf("bad seq number: %d,expect: %d\n", pkt.seqnum,next_ack[prev_ack]);
    //             ack_pkt.acknum = pkt.seqnum;
    //             sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));

    //         }
    //     }
    //     //delay(1);
    //     //buffer[bytes_read] = '\0';
    //     //close(client_socket);

    // }   
    // // fwrite('\0', 1, 1, fp);
    // // fclose(fp);
    // // close(listen_sockfd);
    // // close(send_sockfd);
    // // return 0;
}