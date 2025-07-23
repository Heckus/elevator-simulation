#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

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

//#include "sharedmemory.c"
/**
 * A shared data block.
 */
typedef struct
{
    pthread_mutex_t mutex;           // Locked while accessing struct contents
    pthread_cond_t cond;             // Signalled when the contents can be edited
    char current_floor[4];           // C string in the range B99-B1 and 1-999
    char destination_floor[4];       // Same format as above
    char status[8];                  // C string indicating the elevator's status
    uint8_t open_button;             // 1 if open doors button is pressed, else 0
    uint8_t close_button;            // 1 if close doors button is pressed, else 0
    uint8_t door_obstruction;        // 1 if obstruction detected, else 0
    uint8_t overload;                // 1 if overload detected
    uint8_t emergency_stop;          // 1 if stop button has been pressed, else 0
    uint8_t individual_service_mode; // 1 if in individual service mode, else 0
    uint8_t emergency_mode;          // 1 if in emergency mode, else 0
    
} car_shared_data_t;

/**
 * A shared memory control structure.
 */
typedef struct shared_memory
{
    /// The name of the shared memory object.
    const char *name;

    /// The file descriptor used to manage the shared memory object.
    int fd;

    /// changes made flag
   
    int *shm_changed;                 // 1 if shared memory has changed, else 0

    int shm_send;                    // 1 if car data is needing to be sent to server, else 0
    /// Address of the shared data block.
    car_shared_data_t *data;

} shared_memory_t;



/**
    mutex - initialised with pthread_mutex_init and with the pshared attr enabled
    cond - initialised with pthread_cond_init and with the pshared attr enabled
    current_floor - initialised with the car's lowest floor
    destination_floor - initialised with the car's lowest floor
    status - initialised with Closed
    open_button - initialised with 0
    close_button - initialised with 0
    door_obstruction - initialised with 0
    overload - initialised with 0
    emergency_stop - initialised with 0
    individual_service_mode - initialised with 0
    emergency_mode - initialised with 0
 */


/**
 * Supported operations for the worker process.
 */
typedef enum
{
    OP_NOP,          // Move the elevator up one floor.
    OP_SET_FLOOR,       // Set the current floor.
    OP_MOVE_CURRENT,         // Move the elevator by amount.
    OP_MOVE_DESTINATION, // Move the elevator to the destination floor.
    OP_SET_DESTINATION, // Set the destination floor.
    OP_SET_STATUS,      // Set the elevator status.

    OP_SET_OPEN,   // Set the open doors button.
    OP_CLEAR_OPEN, // Clear the open doors button.

    OP_SET_CLOSE,   // Set the close doors button.
    OP_CLEAR_CLOSE, // Clear the close doors button.

    OP_SET_DOOR_OBSTRUCTION,   // Set the door obstruction flag.
    OP_CLEAR_DOOR_OBSTRUCTION, // Clear the door obstruction flag.

    OP_SET_OVERLOAD,   // Set the door obstruction status.
    OP_CLEAR_OVERLOAD, // Clear the door obstruction status.

    OP_SET_EMERGENCY,   // Set the emergency stop button.
    OP_CLEAR_EMERGENCY, // Clear the emergency stop button.

    OP_SET_SERVICE,   // Set the individual service mode.
    OP_CLEAR_SERVICE, // Clear the individual service mode.

    OP_SET_EMERGENCY_STOP,  // Set the emergency mode.
    OP_CLEAR_EMERGENCY_STOP, // Clear the emergency mode.

    OP_GET_FLOOR,         // Get the current floor.
    OP_GET_DESTINATION,   // Get the destination floor.
    OP_GET_STATUS,        // Get the elevator status.
    OP_GET_OPEN,          // Get the open doors button status.
    OP_GET_CLOSE,         // Get the close doors button status.
    OP_GET_OVERLOAD,      // Get the door obstruction status.
    OP_GET_DOOR_OBSTRUCTION, // Get the door obstruction status.
    OP_GET_EMERGENCY,     // Get the emergency stop button status.
    OP_GET_SERVICE,       // Get the individual service mode status.
    OP_GET_EMERGENCY_STOP // Get the emergency mode status.
} car_op_t;


/**
 * The elevator's status will be one of the following NUL-terminated C strings:

    Opening - indicating that the doors are currently in the process of opening
    Open - indicating that the doors are currently open
    Closing - indicating that the doors are closing
    Closed - indicating that the doors are shut
    Between - indicating that the car is presently between floors
 */
typedef enum
{
    Opening, // 0
    Open,    // 1
    Closing, // 2
    Closed,  // 3
    Between  // 4
} Status;

#define NUM_STATUSES 5
extern const char *status_names[NUM_STATUSES];

/**
 * Supported operations for the worker process.
 */

/**
 * Convert a status string to a Status enum value.
 *
 * @param status_str The status string.
 * @return The corresponding Status enum value, or -1 if the string is invalid.
 */
Status stringToStatus(const char *status_str);

/**
 * Get the label for a floor. Make sure you free the label after use.
 *
 * @param floor The floor number.
 * @return A string representing the floor.
 */
void floorToString(char floorlabel[4], int floor);

/**
 * Convert a floor label back to a floor number.
 *
 * @param floorlabel The string representing the floor.
 * @return The floor number.
 */
#define INT_MIN 999
int stringToFloor(char floorlabel[4]);


bool create_shared_object(shared_memory_t *shm, const char *share_name);

void init_shared_data(shared_memory_t* shm,char lowest[4]);

void destroy_shared_object(shared_memory_t *shm);

bool get_shared_object(shared_memory_t *shm, const char *share_name);

void print_shared_memory(shared_memory_t *shm);





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
bool edit_shared_memory(shared_memory_t *shm, car_op_t operation, int floor,Status status);

/**
     * For a condition operation read_shared_memory(shm, OP_GET_OPEN);
     * 
     * @param shm The shared memory object.
     * @param operation The operation to perform.
     * @return true if the operation was successful, false otherwise.
     */
bool read_shared_memory(shared_memory_t *shm, car_op_t operation);

Status read_car_status(shared_memory_t *shm);

void read_current_floor(shared_memory_t *shm, char current_floor[4]);

void read_destination_floor(shared_memory_t *shm, char destination_floor[4]);

#endif // SHAREDMEMORY_H