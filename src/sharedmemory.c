#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include "sharedmemory.h"


const char *status_names[NUM_STATUSES] = {
    "Opening", // Fits exactly within 7 characters + null terminator
    "Open",    // Fits easily
    "Closing", // Fits easily
    "Closed",  // Fits easily
    "Between"  // Fits exactly within 7 characters + null terminator
};

/**
 * Convert a status string to a Status enum value.
 *
 * @param status_str The status string.
 * @return The corresponding Status enum value, or -1 if the string is invalid.
 */
Status stringToStatus(const char *status_str)
{
    for (int i = 0; i < NUM_STATUSES; i++)
    {
        if (strcmp(status_str, status_names[i]) == 0)
        {
            return (Status)i;
        }
    }
    return -1; // Invalid status string
}

/**
 * Get the label for a floor. Make sure you free the label after use.
 *
 * @param floor The floor number.
 * @return A string representing the floor.
 */
void floorToString(char floorlabel[4], int floor)
{
    
    if (floor < 0)
    {
        // For basement levels, prefix with 'B' and invert the number
        snprintf(floorlabel, 4, "B%d", abs(floor));
    }
    else if(floor == 0){
        snprintf(floorlabel, 4, "%d", 1);
    }
    else
    {
        // For regular floors, just print the number
        snprintf(floorlabel, 4, "%d", floor);
    }
}

/**
 * Convert a floor label back to a floor number.
 *
 * @param floorlabel The string representing the floor.
 * @return The floor number, or INT_MIN if the input is invalid.
 */
int stringToFloor(char floorlabel[4])
{
    int floor = INT_MIN;

    if (floorlabel == NULL)
    {
        return floor;
    }

    if (floorlabel[0] == 'B')
    {
        // For basement levels, convert the number part and negate it
        if (sscanf(floorlabel + 1, "%d", &floor) == 1)
        {
            floor = -floor;
        }
    }
    else
    {
        // For regular floors, just convert the number
        if (sscanf(floorlabel, "%d", &floor) != 1)
        {
            floor = INT_MIN; // Indicate an error if conversion fails
        }
    }

    return floor;
}

bool create_shared_object(shared_memory_t *shm, const char *share_name)
{ // Remove any previous instance of the shared memory object, if it exists.
    shm_unlink(share_name);

    // Assign share name to shm->name.

    
    shm->name = share_name;
    
    // Create the shared memory object, allowing read-write access by all users,
    // and saving the resulting file descriptor in shm->fd. If creation failed,
    // ensure that shm->data is NULL and return false.
    shm->fd = shm_open(share_name, O_RDWR | O_CREAT, 0666);
    if (shm->fd == -1)
    {   
        shm->data = NULL;
        return false;
    }

    // Set the capacity of the shared memory object via ftruncate. If the
    // operation fails, ensure that shm->data is NULL and return false.
    if (ftruncate(shm->fd, sizeof(car_shared_data_t)) == -1)
    {
        close(shm->fd);
        shm->data = NULL;
        return false;
    }

    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data. If mapping fails, return false.
    shm->data = (car_shared_data_t *)mmap(NULL, sizeof(car_shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED)
    {   
        perror("mmap");
        close(shm->fd);
        return false;
    }

    
    // If we reach this point we should return true.
    return true;
}

void init_shared_data(shared_memory_t* shm,char lowest[4]) {

    // Initialize the mutex and condition variable in the shared data block.
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->data->mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shm->data->cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
    
    pthread_mutex_lock(&(shm->data->mutex));
    shm->shm_changed = 1;

    strcpy(shm->data->current_floor,lowest);
    shm->data->open_button = 0;
    strcpy(shm->data->destination_floor,lowest);
    shm->data->close_button = 0;
    strcpy(shm->data->status, status_names[Closed]);    
    shm->data->door_obstruction = 0;
    shm->data->overload = 0;
    shm->data->emergency_stop = 0;
    shm->data->individual_service_mode = 0;
    shm->data->emergency_mode = 0;

    pthread_cond_broadcast(&(shm->data->cond));
    pthread_mutex_unlock(&(shm->data->mutex));
}

void destroy_shared_object(shared_memory_t *shm)
{   
    if (shm->data != NULL) {
        pthread_mutex_destroy(&shm->data->mutex);
        pthread_cond_destroy(&shm->data->cond);
        munmap(shm->data, sizeof(car_shared_data_t));
    }
    if (shm->fd != -1) {
        close(shm->fd);
    }
    shm_unlink(shm->name);
}

bool get_shared_object(shared_memory_t *shm, const char *share_name)
{
    // Get a file descriptor connected to shared memory object and save in
    // shm->fd. If the operation fails, ensure that shm->data is
    // NULL and return false.
    shm->fd = shm_open(share_name, O_RDWR, 0666);
    if (shm->fd == -1)
    {
        shm->data = NULL;
        return false;
    }

    // Otherwise, attempt to map the shared memory via mmap, and save the address
    // in shm->data. If mapping fails, return false.
    shm->data = (car_shared_data_t *)mmap(NULL, sizeof(car_shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->data == MAP_FAILED)
    {
        shm->data = NULL;
        close(shm->fd);
        return false;
    }

    // Modify the remaining stub only if necessary.
    return true;
}

void print_shared_memory(shared_memory_t *shm)
{
    pthread_mutex_lock(&(shm->data->mutex));
    printf("Shared Memory Contents:\n");
    printf("-----------------------\n");
    printf("Current Floor: %s\n", shm->data->current_floor);
    printf("Destination Floor: %s\n", shm->data->destination_floor);
    printf("Status: %s\n", shm->data->status);
    printf("Open Button: %u\n", shm->data->open_button);
    printf("Close Button: %u\n", shm->data->close_button);
    printf("Door Obstruction: %u\n", shm->data->door_obstruction);
    printf("Overload: %u\n", shm->data->overload);
    printf("Emergency Stop: %u\n", shm->data->emergency_stop);
    printf("Individual Service Mode: %u\n", shm->data->individual_service_mode);
    printf("Emergency Mode: %u\n", shm->data->emergency_mode);
    printf("-----------------------\n");
    printf("Car Data:\n");
    printf("-----------------------\n");
    printf("Name: %s\n", shm->name);
    printf("File Descriptor: %d\n", shm->fd);
    printf("Shared Memory Address: %p\n", shm->data);
    printf("Mutex Address: %p\n", &(shm->data->mutex));
    printf("Condition Address: %p\n", &(shm->data->cond));
    printf("Changed: %d\n", shm->shm_changed);
    printf("-----------------------\n");

    pthread_mutex_unlock(&(shm->data->mutex));
}





    /**
     * For a condition operation edit_shared_memory(shm, OP_SET_OPEN, 0, 0);
     * For a floor operation edit_shared_memory(shm, OP_SET_FLOOR, 5, 0);
     * For a status operation edit_shared_memory(shm, OP_SET_STATUS, 0, STATUS_OPEN);
     * 
     * @param shm The shared memory object.
     * @param operation The operation to perform.
     * @param floor The floor number.
     * @param status The status to set.
     * @return true if the operation was successful, false otherwise.
     */
bool edit_shared_memory(shared_memory_t *shm, car_op_t operation, int floor,Status status)
{
    pthread_mutex_lock(&(shm->data->mutex));
    bool retVal = true;
    
    
    switch (operation){
        // Condition operations
        case OP_NOP:
            break;
        case OP_SET_OPEN:
            shm->data->open_button = 1;
            break;
        case OP_CLEAR_OPEN:
            shm->data->open_button = 0;
            break;
        case OP_SET_CLOSE:
            shm->data->close_button = 1;
            break;
        case OP_CLEAR_CLOSE:
            shm->data->close_button = 0;
            break;
        case OP_SET_OVERLOAD:
            shm->data->overload = 1;
            break;
        case OP_CLEAR_OVERLOAD:
            shm->data->overload = 0;
            break;
        case OP_SET_DOOR_OBSTRUCTION:
            shm->data->door_obstruction = 1;
            break;
        case OP_CLEAR_DOOR_OBSTRUCTION:
            shm->data->door_obstruction = 0;
            break;
        case OP_SET_EMERGENCY:
            shm->data->emergency_mode = 1;
            break;
        case OP_CLEAR_EMERGENCY:
            shm->data->emergency_mode = 0;
            break;
        case OP_SET_SERVICE:
            shm->data->individual_service_mode = 1;
            break;
        case OP_CLEAR_SERVICE:
            shm->data->individual_service_mode = 0;
            break;
        case OP_SET_EMERGENCY_STOP:
            shm->data->emergency_stop = 1;
            break;
        case OP_CLEAR_EMERGENCY_STOP:
            shm->data->emergency_stop = 0;
            break;

        // Floor operations
        case OP_MOVE_CURRENT:
            int current_floor = stringToFloor(shm->data->current_floor);
            //printf("Current Floor: %d\n", current_floor);
            //printf("Move: %d\n", floor);
            if (((current_floor+floor)>=0) && (current_floor<0) || ((current_floor+floor)<=0) && (current_floor>0))
            {   
            if (current_floor<0){
                current_floor+=floor;
                current_floor+=1;
            }else if (current_floor>0){
                current_floor+=floor;
                current_floor-=1;
            }
            }else{
                current_floor+=floor;
            }
            //printf("New Floor: %d\n", current_floor);
            floorToString(shm->data->current_floor, current_floor);
            break;
        case OP_MOVE_DESTINATION:
            int destination_floor = stringToFloor(shm->data->destination_floor);
            //printf("destination Floor: %d\n", destination_floor);
            //printf("Move: %d\n", floor);
            if (((destination_floor+floor)>=0) && (destination_floor<0) || ((destination_floor+floor)<=0) && (destination_floor>0))
            {   
            if (destination_floor<0){
                destination_floor+=floor;
                destination_floor+=1;
            }else if (destination_floor>0){
                destination_floor+=floor;
                destination_floor-=1;
            }
            }else{
                destination_floor+=floor;
            }
            //printf("New Floor: %d\n", destination_floor);
            floorToString(shm->data->destination_floor, destination_floor);
            break;
        case OP_SET_FLOOR:
            floorToString(shm->data->current_floor, floor);
            break;
        case OP_SET_DESTINATION:
            floorToString(shm->data->destination_floor, floor);
            break;

        // Status operations
        case OP_SET_STATUS:
            strcpy(shm->data->status, status_names[status]);
            break;

        default:
            printf("Invalid operation.\n");
            retVal = false;
            break;
    }
    shm->shm_changed = 1;
    pthread_cond_broadcast(&(shm->data->cond));
    pthread_mutex_unlock(&(shm->data->mutex));
    return retVal;
}

/**
     * For a condition operation read_shared_memory(shm, OP_GET_OPEN);
     * 
     * @param shm The shared memory object.
     * @param operation The operation to perform.
     * @return true if the operation was successful, false otherwise.
     */
bool read_shared_memory(shared_memory_t *shm, car_op_t operation)
{   
    pthread_mutex_lock(&(shm->data->mutex));
    bool retVal = false;
    switch (operation)
    {
        // Condition operations
        case OP_GET_OPEN:
            if(shm->data->open_button == 1){
                retVal = true;
            }
            break;
        case OP_GET_CLOSE:
            if(shm->data->close_button == 1){
                retVal = true;
            }
            break;
        case OP_GET_OVERLOAD:
            if(shm->data->overload == 1){
                retVal = true;
            }
            break;
        case OP_GET_EMERGENCY:
            if(shm->data->emergency_stop == 1){
                retVal = true;
            }
        case OP_GET_DOOR_OBSTRUCTION:
            if(shm->data->door_obstruction== 1){
                retVal = true;
            }
            break;
        case OP_GET_SERVICE:
            if(shm->data->individual_service_mode == 1){
                retVal = true;
            }
            break;
        case OP_GET_EMERGENCY_STOP:
            if(shm->data->emergency_mode == 1){
                retVal = true;
            }
            break;

        default:
            printf("Invalid operation\n");
            retVal = false;
            break;
    }
    
    pthread_mutex_unlock(&(shm->data->mutex));

    return retVal;
}
Status read_car_status(shared_memory_t *shm)
{
    pthread_mutex_lock(&(shm->data->mutex));
    // Copy the status from shared memory to the provided buffer
    Status carstatus = stringToStatus(shm->data->status);
    pthread_mutex_unlock(&(shm->data->mutex));

    return carstatus;
}

void read_current_floor(shared_memory_t *shm, char current_floor[4])
{
    pthread_mutex_lock(&(shm->data->mutex));
    strcpy(current_floor, shm->data->current_floor);
    pthread_mutex_unlock(&(shm->data->mutex));
}

void read_destination_floor(shared_memory_t *shm, char destination_floor[4])
{
    pthread_mutex_lock(&(shm->data->mutex));
    strcpy(destination_floor, shm->data->destination_floor);
    pthread_mutex_unlock(&(shm->data->mutex));
}