#define _XOPEN_SOURCE 700
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#include "sharedmemory.h"


// TCP Variables
#define PORT 3000
#define BUFFER_SIZE 1024
int clientsockfd;
char carname[256];
uint32_t length;

// Struct for passing data to threads
typedef struct {
    char buffer[BUFFER_SIZE];
    int delaytime;
    ssize_t bytes_read;
    char lowest_floor[4];           // C string in the range B99-B1 and 1-999
    char highest_floor[4];          // Same format as above  

    //flags
    int read_flag;
} thread_data_t;

// Shared memory object for this car
shared_memory_t cardata;
pthread_t doorcontrol_thread_tid, tcp_communication_tid, process_tid;
volatile int thread_stop_signal = 0;
volatile int delaytime = 0;
#define carwait sleep((double)delaytime/1000);
// TCP Function
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

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("inet_pton");
        close(clientsockfd);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    while(connect(clientsockfd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        if(thread_stop_signal) {
            if (clientsockfd != -1){
            close(clientsockfd);
            }       
            exit(EXIT_SUCCESS);
        }
        //printf("Unable to connect to elevator system. Retrying in %0.1lf seconds...\n",(double)delaytime/1000);
        carwait 
    }

    return clientsockfd;
}

// Signal handler for graceful termination
void handle_sigint(int sig) {
    if(sig == 2){
        thread_stop_signal = 1;

        printf("\nCaught signal %d, closing CAR socket and exiting...\n", sig);
        if (clientsockfd != -1) {
            close(clientsockfd);
        }
        //terminate threads
        pthread_cancel(doorcontrol_thread_tid);
        pthread_cancel(tcp_communication_tid);
        pthread_cancel(process_tid);

        pthread_join(doorcontrol_thread_tid, NULL);
        pthread_join(tcp_communication_tid, NULL);
        pthread_join(process_tid, NULL);

        destroy_shared_object(&cardata);
        exit(EXIT_SUCCESS);
    }
}

void add_milliseconds_to_timespec(struct timespec *ts, long milliseconds) {
    ts->tv_sec += milliseconds / 1000;
    ts->tv_nsec += (milliseconds % 1000) * 1000000;
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000;
    }
}

// Thread function for internal control
void *doorcontrol_thread(void *arg) {
    while (thread_stop_signal != 1) {
        //printf("\e[1;1H\e[2J");
        //print_shared_memory(&cardata);
        //carwait
        if(read_shared_memory(&cardata, OP_GET_OPEN)){
            operate_door();
        }
    }
    return NULL;
}

// Thread function for TCP communication
void *tcp_communication_thread(void *arg) {
    thread_data_t *threaddata = arg;
    int connected = 0;

    while (thread_stop_signal != 1) {
        clientsockfd = connect_to_server();

        pthread_mutex_lock(&cardata.data->mutex);
        if(cardata.data->emergency_mode != 1 || cardata.data->individual_service_mode != 1){   
            snprintf(threaddata->buffer, BUFFER_SIZE, "CAR %s %s %s \n", carname, threaddata->lowest_floor, threaddata->highest_floor);
            length = htonl(strlen(threaddata->buffer));
            if (send(clientsockfd, &length, sizeof(length), 0) == -1) {
                perror("send");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }

            if (send(clientsockfd, threaddata->buffer, strlen(threaddata->buffer), 0) == -1) {
                perror("send");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }

            snprintf(threaddata->buffer, BUFFER_SIZE, "STATUS %s %s %s \n", cardata.data->status, cardata.data->current_floor, cardata.data->destination_floor);
            length = htonl(strlen(threaddata->buffer));
            if (send(clientsockfd, &length, sizeof(length), 0) == -1) {
                perror("send");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }

            if (send(clientsockfd, threaddata->buffer, strlen(threaddata->buffer), 0) == -1) {
                perror("send");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }

            connected = 1;
        }
        pthread_mutex_unlock(&cardata.data->mutex);

        while(connected == 1 && thread_stop_signal != 1) {
            pthread_mutex_lock(&cardata.data->mutex);
            if(cardata.data->emergency_mode == 1){
                snprintf(threaddata->buffer, BUFFER_SIZE, "EMERGENCY");
                length = htonl(strlen(threaddata->buffer));
                if (send(clientsockfd, &length, sizeof(length), 0) == -1) {
                    perror("send");
                    connected = 0;
                    pthread_mutex_unlock(&cardata.data->mutex);
                    close(clientsockfd);
                    continue;
                }

                if (send(clientsockfd, threaddata->buffer, strlen(threaddata->buffer), 0) == -1) {
                    perror("send");
                    connected = 0;
                    pthread_mutex_unlock(&cardata.data->mutex);
                    close(clientsockfd);
                    continue;
                }

                close(clientsockfd);
                connected = 0;
            }

            if(cardata.data->individual_service_mode == 1){
                snprintf(threaddata->buffer, BUFFER_SIZE, "INDIVIDUAL SERVICE");
                length = htonl(strlen(threaddata->buffer));
                if (send(clientsockfd, &length, sizeof(length), 0) == -1) {
                    perror("send");
                    connected = 0;
                    pthread_mutex_unlock(&cardata.data->mutex);
                    close(clientsockfd);
                    continue;
                }

                if (send(clientsockfd, threaddata->buffer, strlen(threaddata->buffer), 0) == -1) {
                    perror("send");
                    connected = 0;
                    pthread_mutex_unlock(&cardata.data->mutex);
                    close(clientsockfd);
                    continue;
                }

                close(clientsockfd);
                connected = 0;
            }

            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            add_milliseconds_to_timespec(&ts, delaytime);
            while (cardata.shm_send != 1) {
                int rc = pthread_cond_timedwait(&cardata.data->cond, &cardata.data->mutex, &ts);
                if (rc == ETIMEDOUT) {
                    break;
                } else if (rc != 0) {
                    perror("pthread_cond_timedwait");
                    pthread_mutex_unlock(&cardata.data->mutex);
                    break;
                }
            }

            snprintf(threaddata->buffer, BUFFER_SIZE, "STATUS %s %s %s \n", cardata.data->status, cardata.data->current_floor, cardata.data->destination_floor);
            length = htonl(strlen(threaddata->buffer));
            if (send(clientsockfd, &length, sizeof(length), 0) == -1) {
                perror("send");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }

            if (send(clientsockfd, threaddata->buffer, strlen(threaddata->buffer), 0) == -1) {
                perror("send");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }
            
            //Set the socket to non-blocking mode
            int flags = fcntl(clientsockfd, F_GETFL, 0);
            if (flags == -1) {
                perror("fcntl");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }
            if (fcntl(clientsockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
                perror("fcntl");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }
            uint32_t full_length;
            threaddata->bytes_read = recv(clientsockfd, &full_length, sizeof(full_length), MSG_DONTWAIT);
            if (threaddata->bytes_read <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available, continue the loop
                    pthread_mutex_unlock(&cardata.data->mutex);
                    continue;
                } else {
                    perror("recv");
                    connected = 0;
                    pthread_mutex_unlock(&cardata.data->mutex);
                    close(clientsockfd);
                    continue;
                }
            }
            
            uint32_t response_length = ntohl(full_length);
            if (response_length == 0 || response_length > BUFFER_SIZE) {
                perror("recv");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }

            threaddata->bytes_read = recv(clientsockfd, threaddata->buffer, response_length, MSG_WAITALL);
            if (threaddata->bytes_read <= 0) {
                perror("recv");
                connected = 0;
                pthread_mutex_unlock(&cardata.data->mutex);
                close(clientsockfd);
                continue;
            }

            threaddata->buffer[response_length] = '\0';
            if (threaddata->read_flag == 1) {
                sscanf(threaddata->buffer, "FLOOR %s ", cardata.data->destination_floor);
            }

            printf("Received destination floor from server: %s\n", threaddata->buffer);
            cardata.shm_send = 1;
            pthread_mutex_unlock(&cardata.data->mutex);
        }
    }
    return NULL;
}











void change_status(Status *status, Status new_status) {*status = new_status;}
void operate_door(void) {
    Status current_status = Opening;
    Status next_status;
    while (thread_stop_signal != 1) {
        // Check if status is correct
        if(strcmp(status_names[read_car_status(&cardata)], status_names[current_status]) != 0){
            edit_shared_memory(&cardata, OP_SET_STATUS, 0, current_status);
        }
        // Check if open button is pressed
        if(read_shared_memory(&cardata, OP_GET_OPEN) == 1){
            if(current_status == Closed || current_status == Closing){
                current_status = Opening;
                edit_shared_memory(&cardata, OP_CLEAR_OPEN, 0, 0);
            }
            if(current_status == Opening || current_status == Between){
                edit_shared_memory(&cardata, OP_CLEAR_OPEN, 0, 0);
            }
            if(current_status == Open){
                current_status = Closing;
                carwait
                edit_shared_memory(&cardata, OP_CLEAR_OPEN, 0, 0);
            }
        }
        // Check if close button is pressed
        if(read_shared_memory(&cardata, OP_GET_CLOSE) == 1){
            if(current_status == Open){
                current_status = Closing;
                edit_shared_memory(&cardata, OP_CLEAR_CLOSE, 0, 0);
            }
        }

        switch (current_status){
            case Opening:
                edit_shared_memory(&cardata, OP_SET_STATUS, 0, Opening);
                //printf("Door is opening\n");
                next_status = Open;
                break;
            case Open:
                edit_shared_memory(&cardata, OP_SET_STATUS, 0, Open);
                //printf("Door is open\n");
                next_status = Closing;
                break;
            case Closing:
                edit_shared_memory(&cardata, OP_SET_STATUS, 0, Closing);
                //printf("Door is closing\n");
                next_status = Closed;
                break;
            case Closed:
                edit_shared_memory(&cardata, OP_SET_STATUS, 0, Closed);
                //printf("Door is closed\n");
                next_status = Opening;
                break;
        }

        if (current_status == Closed && next_status == Opening) {
            break;
        }
        change_status(&current_status, next_status);
        carwait
    }
}


// Thread function for process
void *process_thread(void *arg){
    thread_data_t *threaddata;
    threaddata = arg;
    threaddata->read_flag = 1;

    while(thread_stop_signal != 1) {

        pthread_mutex_lock(&cardata.data->mutex);
        int at_destination = strcmp(cardata.data->destination_floor, cardata.data->current_floor);
        int floordiff = stringToFloor(cardata.data->destination_floor) - stringToFloor(cardata.data->current_floor);
        //make sure destination is not greater than highest floor
        if (stringToFloor(cardata.data->destination_floor) > stringToFloor(threaddata->highest_floor)){
            strcpy(cardata.data->destination_floor, cardata.data->current_floor);
        }
        //make sure destination is not less than lowest floor
        if (stringToFloor(cardata.data->destination_floor) < stringToFloor(threaddata->lowest_floor)){
            strcpy(cardata.data->destination_floor, cardata.data->current_floor);
        }
        
        pthread_mutex_unlock(&cardata.data->mutex);
        if (cardata.shm_send == 1 && at_destination == 0) {
            operate_door();
            cardata.shm_send = 0;
        }

        if (at_destination != 0 && read_car_status(&cardata) == Closed){
            //move to destination floor process
            for (int i = 0; i < abs(floordiff); i++) {
                edit_shared_memory(&cardata, OP_SET_STATUS, 0, Between);
                threaddata->read_flag = 0;
                //printf("Car is moving\n");
                carwait
                if (floordiff > 0) {
                    edit_shared_memory(&cardata, OP_MOVE_CURRENT, 1, 0);
                    edit_shared_memory(&cardata, OP_SET_STATUS, 0, Closed);
                    threaddata->read_flag = 1;
                } else{
                    edit_shared_memory(&cardata, OP_MOVE_CURRENT, -1, 0);
                    edit_shared_memory(&cardata, OP_SET_STATUS, 0, Closed);
                    threaddata->read_flag = 1;
                }
            }
            operate_door();
       }
    }
    return NULL;
}


































int main(int argc, char *argv[]) {
    signal(SIGINT,handle_sigint);
    signal(SIGPIPE, SIG_IGN); //-> errno to EPIPE
    atexit(handle_sigint);
    //TCP Variables
    thread_data_t thread_data;
    //Thread variables
    
    
    // Input validation
    // Validate right amount of arguments
    if (argc != 5) {
        fprintf(stderr, "Usage: %s {name} {lowest floor} {highest floor} {delay} \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // // Validate name to ensure it only contains alphanumeric characters
    // for (int i = 0; argv[1][i] != '\0'; i++) {
    //     if (!isalpha(argv[1][i])) {
    //         fprintf(stderr, "Error: Name must only contain alphanumeric characters.\n");
    //         exit(EXIT_FAILURE);
    //     }
    // }

    // Parse and validate delay time
    thread_data.delaytime = atoi(argv[4]);
    delaytime = thread_data.delaytime;
    if (thread_data.delaytime <= 0) {
        fprintf(stderr, "Error: Delay time must be a positive integer.\n");
        exit(EXIT_FAILURE);
    }
    
    // Check if floor numbers are valid
    if (strlen(argv[2]) >= 4 || strlen(argv[3]) >= 4) {
        fprintf(stderr, "Invalid floor(s) length specified.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 4; i++) {
        if (!(argv[2][i] >= '0' || argv[2][i] <= '9' || argv[2][i] == 'B')) {
            fprintf(stderr, "Invalid floor(s) ! specified. %s %s\n",argv[2],argv[3]);
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < 4; i++) {
        if (!(argv[3][i] >= '0' || argv[3][i] <= '9' || argv[3][i] == 'B')) {
            fprintf(stderr, "Invalid floor(s) specified.\n");
            exit(EXIT_FAILURE);
        }
    }

    // Add lowest and highest floor to thread_data
    strcpy(thread_data.lowest_floor, argv[2]);
    strcpy(thread_data.highest_floor, argv[3]);

    // Create shared memory object
    char shm_name[256];
    strcpy(carname, argv[1]);
    snprintf(shm_name, sizeof(shm_name), "/car%s", argv[1]);
    if (create_shared_object(&cardata, shm_name) == false) {
        fprintf(stderr, "Error: Failed to create shared memory object.\n");
        exit(EXIT_FAILURE);
    }
       
    //Initialise shared memory object
    init_shared_data(&cardata,argv[2]);
    

    // Create threads
    if (pthread_create(&doorcontrol_thread_tid, NULL, doorcontrol_thread, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&tcp_communication_tid, NULL, tcp_communication_thread, &thread_data) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&process_tid, NULL, process_thread, &thread_data) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    
    
    //this will be waiting for the threads to finish so like a for loop
    pthread_join(doorcontrol_thread_tid, NULL);
    pthread_join(tcp_communication_tid, NULL);
    pthread_join(process_tid, NULL);
    destroy_shared_object(&cardata);
    if (clientsockfd != -1) {
        close(clientsockfd);
    }
    // Close the socket if all threads have finished(wont happen in this case)
    

    return 0;
}
