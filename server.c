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
    int expected_seq_num = 0;
    int recv_len;
    struct packet ack_pkt;

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


    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt
    

    while (1) {
        
        char buffer[PAYLOAD_SIZE];
        ssize_t bytes_read;
        struct packet pkt;
    

        if (bytes_read = recvfrom(listen_sockfd, (void *) &pkt, sizeof(pkt), 0, &server_addr, sizeof(server_addr)) > 0) {
            printRecv(&pkt);

            // if seq number is prev_ack's next or starting new file
            if(prev_ack==-1 || pkt.seqnum == next_ack[prev_ack])
            {
                // copy current content to 
                char payload[PAYLOAD_SIZE];
                unsigned int length = pkt.length;
                memcpy(payload, pkt.payload, length);
                fwrite(payload, length, 1, fp);

                // send ack number back 
                ack_pkt.acknum = pkt.acknum;
                prev_ack = ack_pkt.acknum;
                sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));
            }
            else{
                // send ack number back 
                sendto(send_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, &client_addr_to, sizeof(client_addr_to));
            }
        }


        //buffer[bytes_read] = '\0';
        //close(client_socket);
    }   

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
