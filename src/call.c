#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "sharedmemory.h"

#define PORT 3000
#define BUFFER_SIZE 1024
#define IP "127.0.0.1"
int clientsockfd;

// Function to create a socket and connect to the server
int connect_to_server() {
    struct sockaddr_in server_address;

    // Create socket
    clientsockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientsockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Prepare the sockaddr_in structure
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    
    //Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, IP, &server_address.sin_addr) <= 0) {
        perror("inet_pton");
        close(clientsockfd);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
   

    if (connect(clientsockfd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        printf("Unable to connect to elevator system.\n");
        close(clientsockfd);
        exit(EXIT_FAILURE);
    }
    
    return clientsockfd;
}

// Signal handler for graceful termination
void handle_sigint(int sig) {
    printf("\nCaught signal %d, closing server socket and exiting...\n", sig);
    close(clientsockfd);
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {
    signal(SIGINT,handle_sigint);
    //TCP Variables
    int clientsockfd;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    //Input validation
    if (argc != 3) {
        fprintf(stderr, "Usage: %s {source floor} {destination floor}\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Check if argv[1] and argv[2] are valid
    if (strlen(argv[1]) >= 4 || strlen(argv[2]) >= 4) {
        fprintf(stderr, "Invalid floor(s) specified.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 4; i++) {
        if (!(argv[1][i] >= '0' || argv[1][i] <= '9' || argv[1][i] == 'B')) {
            fprintf(stderr, "Invalid floor(s) specified.\n");
            exit(EXIT_FAILURE);
        }
        if (!(argv[2][i] >= '0' || argv[2][i] <= '9' || argv[2][i] == 'B')) {
            fprintf(stderr, "Invalid floor(s) specified.\n");
            exit(EXIT_FAILURE);
        }
    }

    int sourcefloor = stringToFloor(argv[1]);
    //printf("Source floor: %d\n", sourcefloor);
    int destinationfloor = stringToFloor(argv[2]);
    //printf("Destination floor: %d\n", destinationfloor);

    if(sourcefloor == destinationfloor){
        printf("You are already on that floor!\n");
        exit(EXIT_FAILURE);
    }

   
    // Connection to the server

    clientsockfd = connect_to_server();
    
    // int flags = fcntl(clientsockfd, F_GETFL, 0);
    // fcntl(clientsockfd, F_SETFL, flags | O_NONBLOCK);

    //printf("Connected to server at 127.0.0.1:%d\n", PORT);

    // Communicate with the server
    while (1) {
        // Read message from stdin
        //snprintf(buffer, BUFFER_SIZE, "CALL %s %s \n", sourcefloor, destinationfloor);
        snprintf(buffer, BUFFER_SIZE, "CALL %s %s \n", argv[1], argv[2]);

        //WRITE
        uint32_t length = htonl(strlen(buffer));
        if (send(clientsockfd, &length, sizeof(length), 0) == -1) {
            perror("send");
            close(clientsockfd);
            break;
        }

        if (send(clientsockfd, buffer, strlen(buffer), 0) == -1) {
            perror("send");
            close(clientsockfd);
            break;
        }
        
        //READ
        uint32_t full_length;
        ssize_t bytes_read = recv(clientsockfd, &full_length, sizeof(full_length), MSG_WAITALL);
        if (bytes_read <= 0) {
        printf("Unable to connect to elevator system.\n");
        close(clientsockfd);
        break;
        }
        uint32_t response_length = ntohl(full_length);
        if (response_length == 0 || response_length > 1024) {
        printf("Invalid response from elevator system.\n");
        close(clientsockfd);
        break;
        }
        // Null-terminate the buffer and print the response
        char buffer[response_length + 1];
        bytes_read = recv(clientsockfd, buffer, response_length, MSG_WAITALL);
        if (bytes_read <= 0) {
            printf("Unable to connect to elevator system.\n");
            close(clientsockfd);
            break;
        }
        buffer[response_length] = '\0';  // Null-terminate response string


        if (buffer[0] == 'U') {
            printf("Sorry, no car is available to take this request.\n");
            printf("Received message from server: %s \n", buffer);
            close(clientsockfd);
            break;
        } else if (buffer[0] == 'C') {
            // Assuming the response format is "CAR {car name}"
            char car_name[BUFFER_SIZE];
            sscanf(buffer, "CAR %s", car_name);
            printf("Car %s is on its way to pick you up.\n", car_name);
            close(clientsockfd);
            break;
        }
        printf("Received message from server: %s \n", buffer);
        close(clientsockfd);
        break;
        
    }

    // Close the socket
    close(clientsockfd);

    return 0;
}
