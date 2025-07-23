#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#include "sharedmemory.h"

shared_memory_t cardata;

// Signal handler for graceful termination
void handle_sigint(int sig) {
    printf("\nCaught signal %d, closing CAR INTERNAL and exiting...\n", sig);
    munmap(cardata.data, sizeof(car_shared_data_t));
    close(cardata.fd);
    exit(EXIT_SUCCESS);
}


typedef enum
{
    opendoor,
    closedoor,
    stop,
    serviceon,
    serviceoff,
    up,
    down

} input;

input get_operation(const char *operation) {
    if (strcmp(operation, "open") == 0) {
        return opendoor;
    } else if (strcmp(operation, "close") == 0) {
        return closedoor;
    } else if (strcmp(operation, "stop") == 0) {
        return stop;
    } else if (strcmp(operation, "service_on") == 0) {
        return serviceon;
    } else if (strcmp(operation, "service_off") == 0) {
        return serviceoff;
    } else if (strcmp(operation, "up") == 0) {
        return up;
    } else if (strcmp(operation, "down") == 0) {
        return down;
    } else {
        fprintf(stderr, "Invalid operation.\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT,handle_sigint);
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s {car name} {operation} \n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // // Validate name to ensure it only contains alphanumeric characters
    // for (int i = 0; argv[1][i] != '\0'; i++) {
    //     if (!isalpha(argv[1][i])) {
    //         fprintf(stderr, "Error: Name must only contain alphanumeric characters.\n");
    //         exit(EXIT_FAILURE);
    //     }
    // }
    // Check if floor numbers are valid
    if (strlen(argv[2]) >= 12) {
        fprintf(stderr, "Error: Invalid operation\n");
        exit(EXIT_FAILURE);
    }

    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/car%s", argv[1]);
    if(get_shared_object(&cardata, shm_name) == false) {
        fprintf(stderr, "Unable to access car %s.\n",argv[1]);
        exit(EXIT_FAILURE);
    }

    /**
     * The operation is one of the following:

        open - sets open_button in the shared memory segment to 1
        close - sets close_button in the shared memory segment to 1
        stop - sets emergency_stop in the shared memory segment to 1
        service_on - sets individual_service_mode in the shared memory segment to 1 and emergency_mode to 0
        service_off - sets individual_service_mode in the shared memory segment to 0
        up - sets the destination floor to the next floor up from the current floor. Only usable in individual service mode, when the elevator is not moving and the doors are closed
        down - sets the destination floor to the next down from the current floor. Only usable in individual service mode, when the elevator is not moving and the doors are closed
    */
    input opp = get_operation(argv[2]);


    switch(opp){
        case opendoor:
            if(read_car_status(&cardata) == 4){
                fprintf(stderr, "Operation not allowed while elevator is moving.\n");
                exit(EXIT_FAILURE);
            }
            else{
                edit_shared_memory(&cardata,OP_SET_OPEN,0,0);
            }
            break;
        case closedoor:
            edit_shared_memory(&cardata,OP_SET_CLOSE,0,0);
            break;
        case stop:
            edit_shared_memory(&cardata,OP_SET_EMERGENCY_STOP,0,0);
            break;
        case serviceon:
            edit_shared_memory(&cardata,OP_SET_SERVICE,0,0);
            edit_shared_memory(&cardata,OP_CLEAR_EMERGENCY,0,0);
            edit_shared_memory(&cardata,OP_CLEAR_EMERGENCY_STOP,0,0);
            edit_shared_memory(&cardata,OP_CLEAR_OVERLOAD,0,0);
            edit_shared_memory(&cardata,OP_CLEAR_DOOR_OBSTRUCTION,0,0);
            break;
        case serviceoff:
            edit_shared_memory(&cardata,OP_CLEAR_SERVICE,0,0);
            break;
        case up:
            if(!read_shared_memory(&cardata,OP_GET_SERVICE)){
                fprintf(stderr, "Operation only allowed in service mode.\n");
                exit(EXIT_FAILURE);
            }else if(read_car_status(&cardata) == 1){
                fprintf(stderr, "Operation only allowed when doors are open.\n");
                exit(EXIT_FAILURE);
            }else if(read_car_status(&cardata) == 4){
                fprintf(stderr, "Operation not allowed while elevator is moving.\n");
                exit(EXIT_FAILURE);
            }else{
                edit_shared_memory(&cardata,OP_MOVE_DESTINATION,1,0);
            }
            break;
        case down:
            if(!read_shared_memory(&cardata,OP_GET_SERVICE)){
                fprintf(stderr, "Operation only allowed in service mode.\n");
                exit(EXIT_FAILURE);
            }else if(read_car_status(&cardata) == 1){
                fprintf(stderr, "Operation only allowed when doors are open.\n");
                exit(EXIT_FAILURE);
            }else if(read_car_status(&cardata) == 4){
                fprintf(stderr, "Operation not allowed while elevator is moving.\n");
                exit(EXIT_FAILURE);
            }else{
                edit_shared_memory(&cardata,OP_MOVE_DESTINATION,-1,0);
            }
            break;
        default:
            fprintf(stderr, "Invalid operation.\n");
            exit(EXIT_FAILURE);
    }
    return 0;
}