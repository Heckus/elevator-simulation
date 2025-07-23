
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <errno.h>
#include "controllermemory.h"

#define PORT 3000
#define BACKLOG 10
#define BUFFER_SIZE 1024


typedef struct {
    int server_sockfd;
    controller_t controller;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
}controller_data_t;

controller_data_t controller_data;
pthread_t  tcp_communication_tid, process_tid;
volatile int thread_stop_signal = 0;


// TCP Functions
// Function to create a socket
int create_socket(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt_enable = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable))) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

   
// Function to bind the socket to an address and port
void bind_socket(int sockfd, struct sockaddr_in *address) {
    if (bind(sockfd, (struct sockaddr *)address, sizeof(*address)) == -1) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
}

// Function to listen for incoming connections
void listen_socket(int sockfd) {
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
}

// Function to accept an incoming connection
int accept_connection(int sockfd, struct sockaddr_in *client_address) {
    socklen_t client_len = sizeof(*client_address);
    int client_sockfd = accept(sockfd, (struct sockaddr *)client_address, &client_len);
    if (client_sockfd == -1) {
        perror("accept");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    return client_sockfd;
}

bool handle_elevator_call(controller_data_t *controller_data, int source_floor, int dest_floor, char* selected_car_name, fd_set *master_set) {
    if (controller_data == NULL) {
        fprintf(stderr, "Invalid controller_data pointer\n");
        return false;
    }
    if (controller_data->controller.data == NULL) {
        fprintf(stderr, "Invalid controller data\n");
        return false;
    }

    connectedcar_t* best_car = NULL;
    int min_distance = 100;

    // Find the best car to handle the request
    for (size_t i = 0; i < controller_data->controller.size; i++) {
        connectedcar_t* car = &controller_data->controller.data[i];

        if (!can_service_request(car, source_floor, dest_floor)) {
            continue;
        }

        // Calculate distance score
        int current_floor = stringToFloor(car->currentfloor);
        int distance = abs(current_floor - source_floor);

        // Prefer cars already moving in the right direction
        Direction request_direction = (dest_floor > source_floor) ? DIRECTION_UP : DIRECTION_DOWN;
        if (car->current_direction == request_direction) {
            distance -= 5; // Bonus for matching direction
        }

        // Prefer cars with shorter queues
        QueueNode* temp = car->queue_head;
        int queue_length = 0;
        while (temp) {
            queue_length++;
            if (temp != NULL) {
                temp = temp->next;
            }
        }
        distance += queue_length * 2;

        if (distance < min_distance) {
            min_distance = distance;
            best_car = car;
        }
    }

    if (best_car) {
        if (add_to_car_queue(best_car, source_floor, dest_floor)) {
            strcpy(selected_car_name, best_car->name);
            printf("Selected car: %s\n", best_car->name);

            // Send new destination if queue was empty
            if (strcmp(best_car->destinationfloor, best_car->currentfloor) == 0) {
                int next_dest = get_next_destination(best_car);
                char next_dest_string[4];
                floorToString(next_dest_string, next_dest);
                printf("Next destination: %s\n", next_dest_string);

                if (next_dest != -1) {
                    snprintf(best_car->destinationfloor, sizeof(best_car->destinationfloor), "%d", next_dest);
                    snprintf(controller_data->buffer, BUFFER_SIZE, "FLOOR %s", next_dest_string);

                    int car_socket = controller_get_socket_by_name(&controller_data->controller, best_car->name);
                    if (car_socket == -1) {
                        fprintf(stderr, "Error: Could not find socket for car %s\n", best_car->name);
                        return false;
                    }

                    uint32_t length = htonl(strlen(controller_data->buffer));
                    if (send(car_socket, &length, sizeof(length), 0) == -1) {
                        perror("send");
                        close(car_socket);
                        FD_CLR(car_socket, master_set);
                    }
                    if (send(car_socket, controller_data->buffer, strlen(controller_data->buffer), 0) == -1) {
                        perror("send");
                        close(car_socket);
                        FD_CLR(car_socket, master_set);
                    }  
                }
            }
            return true;
        }
    }

    return false;
}



// Signal handler for graceful termination
void handle_sigint(int sig) {
    printf("\nCaught signal %d, closing server socket and exiting...\n", sig);
    thread_stop_signal = 1;
    // pthread_cancel(diagnostic_thread_tid);
    pthread_cancel(tcp_communication_tid);
    pthread_cancel(process_tid);
    pthread_join(tcp_communication_tid, NULL);
    pthread_join(process_tid, NULL);
    controller_destroy(&controller_data.controller);
    exit(EXIT_SUCCESS);
}



// Thread function for process
void *process_thread(void *arg){
    controller_data_t *controller_data;
    controller_data = arg;
    while(thread_stop_signal != 1) {
        sleep(2);
        //system("cls");
        //printf("\e[1;1H\e[2J");
        //controller_print(&controller_data->controller);
    }
    return NULL;
}

void *tcp_communication_thread(void *arg) {
    controller_data_t *controller_data;
    controller_data = arg;

    int client_sockfd;
    struct sockaddr_in server_address, client_address;
    controller_data->server_sockfd = create_socket();
    
    // Prepare the sockaddr_in structure
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    // Bind the socket
    bind_socket(controller_data->server_sockfd, &server_address);

    // Listen for incoming connections
    listen_socket(controller_data->server_sockfd);
    printf("Server listening on port %d\n", PORT);

    // Initialize the master and temp sets
    fd_set master_set, read_fds;
    int fdmax, i;
    

    FD_ZERO(&master_set);
    FD_ZERO(&read_fds);

    // Add the server socket to the master set
    FD_SET(controller_data->server_sockfd, &master_set);
    fdmax = controller_data->server_sockfd;

    while (thread_stop_signal != 1) {
        read_fds = master_set; // Copy the master set to the temp set

        // Use select to wait for activity on any of the sockets
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // Loop through the file descriptors to check which ones are ready
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == controller_data->server_sockfd) {
                    // Handle new connections
                    client_sockfd = accept_connection(controller_data->server_sockfd, &client_address);
                    printf("New connection accepted\n");

                    // Add the new client socket to the master set
                    FD_SET(client_sockfd, &master_set);
                    if (client_sockfd > fdmax) {
                        fdmax = client_sockfd;
                    }
                } else {
                    // Handle data from a client
                    
                    uint32_t full_length;
                    controller_data->bytes_read = recv(i, &full_length, sizeof(full_length), MSG_WAITALL);
                    if (controller_data->bytes_read <= 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // No data available, continue the loop
                        continue;
                        } else 
                        if (controller_data->bytes_read == 0) {
                           
                            char* name = controller_get_name_by_socket(&controller_data->controller, i);
                            controller_remove_by_name(&controller_data->controller, name);
                            if (name != NULL){printf("Socket %s hung up \n", name);}else{printf("Socket callpad hung up \n");}
                            close(i);
                            FD_CLR(i, &master_set);
                        } else {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &master_set);
                        continue;
                    }
              




                    uint32_t response_length = ntohl(full_length);
                    if (response_length == 0) {
                        printf("Socket %d hung up \n", i);
                        close(i);
                        FD_CLR(i, &master_set);
                    }

                    // Null-terminate the buffer
                    controller_data->buffer[response_length + 1];

                    controller_data->bytes_read = recv(i, controller_data->buffer, response_length, MSG_WAITALL);
                    if (controller_data->bytes_read <= 0) {
                        if (controller_data->bytes_read == 0) {
                            printf("Socket %d hung up \n", i);
                            close(i);
                            FD_CLR(i, &master_set);
                        } else {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &master_set);
                    }
                    controller_data->buffer[response_length] = '\0';  // Null-terminate response string

                    // Process the received data
                    char command[5];
                    strncpy(command, controller_data->buffer, 4);
                    command[4] = '\0';

                    switch (command[0]) {
                        case 'C':
                            if (strcmp(command, "CALL") == 0) {
                                char source[4], destination[4], selected_car[50];
                                sscanf(controller_data->buffer, "CALL %s %s", source, destination);

                                if (handle_elevator_call(controller_data, stringToFloor(source), stringToFloor(destination), selected_car,&master_set)) {
                                    snprintf(controller_data->buffer, BUFFER_SIZE, "CAR %s", selected_car);
                                } else {
                                    strncpy(controller_data->buffer, "UNAVAILABLE", BUFFER_SIZE);
                                }

                                uint32_t length = htonl(strlen(controller_data->buffer));
                                if (send(i, &length, sizeof(length), 0) == -1) {
                                    perror("send");
                                    close(i);
                                    FD_CLR(i, &master_set);
                                    continue;
                                }
                                if (send(i, controller_data->buffer, strlen(controller_data->buffer), 0) == -1) {
                                    perror("send");
                                    close(i);
                                    FD_CLR(i, &master_set);
                                    continue;
                                }
                            } else if (strcmp(command, "CAR ") == 0) {
                                char name[50], lowest_floor[4], highest_floor[4];
                                sscanf(controller_data->buffer, "CAR %s %s %s", name, lowest_floor, highest_floor);

                                connectedcar_t car;
                                strncpy(car.name, name, sizeof(car.name) - 1);
                                strncpy(car.lowest_floor, lowest_floor, sizeof(car.lowest_floor) - 1);
                                strncpy(car.highest_floor, highest_floor, sizeof(car.highest_floor) - 1);
                                strncpy(car.previous_status, "IDLE", sizeof(car.previous_status) - 1);
                                car.previous_status[sizeof(car.previous_status) - 1] = '\0';
                                car.name[sizeof(car.name) - 1] = '\0';
                                car.lowest_floor[sizeof(car.lowest_floor) - 1] = '\0';
                                car.highest_floor[sizeof(car.highest_floor) - 1] = '\0';
                                car.connectionsocket = i;
                                controller_push(&controller_data->controller, &car);
                            }
                            break;
                        case 'S':
                            if (strcmp(command, "STAT") == 0) {
                                char status[8], current_floor[4], destination_floor[4];
                                const char* name = controller_get_name_by_socket(&controller_data->controller, i);
                                if (name != NULL) {
                                    sscanf(controller_data->buffer, "STATUS %7s %3s %3s", status, current_floor, destination_floor);

                                    // Update car status
                                    controller_set(&controller_data->controller, name, 3, status);
                                    controller_set(&controller_data->controller, name, 4, current_floor);
                                    controller_set(&controller_data->controller, name, 5, destination_floor);

                                    for (size_t j = 0; j < controller_data->controller.size; j++) {
                                        if (strcmp(controller_data->controller.data[j].name, name) == 0) {
                                            connectedcar_t* car = &controller_data->controller.data[j];

                                            // Update elevator status
                                            // If car has arrived at destination, remove from queue and get next destination
                                            if (strcmp(car->previous_status, "Closing") == 0 && strcmp(car->status, "Closed") == 0) {
                                                remove_from_car_queue(car);
                                                int next_dest = get_next_destination(car);
                                                char next_dest_str[4];
                                                floorToString(next_dest_str, next_dest);
                                                if (next_dest != -1) {
                                                    snprintf(controller_data->buffer, BUFFER_SIZE, "FLOOR %s", next_dest_str);

                                                    uint32_t length = htonl(strlen(controller_data->buffer));
                                                    if (send(i, &length, sizeof(length), 0) == -1) {
                                                        perror("send");
                                                        close(i);
                                                        FD_CLR(i, &master_set);
                                                        continue;
                                                    }
                                                    if (send(i, controller_data->buffer, strlen(controller_data->buffer), 0) == -1) {
                                                        perror("send");
                                                        close(i);
                                                        FD_CLR(i, &master_set);
                                                        continue;
                                                    }
                                                }
                                            }
                                            break;
                                        }
                                    }
                                    controller_set(&controller_data->controller, name, 7, status);
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
}














int main(void){
    signal(SIGINT, handle_sigint);

    
    // Register signal handler for SIGINT (CTRL + C)
    controller_init(&controller_data.controller);
    // Create socket
    if (pthread_create(&tcp_communication_tid, NULL, tcp_communication_thread, &controller_data) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&process_tid, NULL, process_thread, &controller_data) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(tcp_communication_tid, NULL);
    pthread_join(process_tid, NULL);
    controller_destroy(&controller_data.controller);
    // Close the server socket (this line will never be reached due to the infinite loop)

    return 0;
}
