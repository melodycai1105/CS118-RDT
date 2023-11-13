#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd, client_sockfd;
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
    int next_seq [] = {1, 2, 0};
    int next_ack [] = {0, 1, 2};

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
    while((bytes_read = fread(buffer, 1, PAYLOAD_SIZE-1, fp))> 0){
        buffer[bytes_read] = '\0';
        // printf("%s", buffer);
        build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read + 1, (const char*) buffer);
        sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
        recvfrom(listen_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        while (ack_pkt.acknum != next_ack[seq_num]){
            sendto(send_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
            recvfrom(listen_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
        }
        // ssize_t read;
        // while ((read = recv(client_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0)) <= 0){
        //     if (read < 0){
        //         perror("recv error\n");
        //         return 1;
        //     }
        // }
        seq_num = next_seq[seq_num];
    }
    
    close(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

