#ifndef CONTROLLERMEMORY_H
#define CONTROLLERMEMORY_H


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "sharedmemory.h"

typedef enum {
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_IDLE
} Direction;

// Queue node for floor requests
typedef struct QueueNode {
    int floor;
    Direction direction;
    struct QueueNode* next;
} QueueNode;



typedef struct connectedcar {
    char name[50];
    char highest_floor[4];
    char lowest_floor[4];
    char status[8];
    char previous_status[8];
    char currentfloor[4];
    char destinationfloor[4];
    int connectionsocket;
    int available;

    QueueNode* queue_head;
    Direction current_direction;
    Status elevator_status;
} connectedcar_t;

typedef struct controller {
    size_t size;
    size_t capacity;
    connectedcar_t* data;
} controller_t;



void queue_init(connectedcar_t* car);

bool can_service_request(connectedcar_t* car, int source_floor, int dest_floor);



bool add_to_car_queue(connectedcar_t* car, int source_floor, int dest_floor);


// Function to get next destination floor for a car
int get_next_destination(connectedcar_t* car);

// Function to remove current floor from queue when reached
void remove_from_car_queue(connectedcar_t* car);





/**
 * @brief Initializes the controller with an initial capacity.
 * 
 * @param controller A pointer to the controller to initialize.
 */
void controller_init(controller_t* controller);

/**
 * @brief Ensures that the controller has enough capacity to store new data.
 * 
 * @param controller A pointer to the controller.
 * @param new_size The new size that needs to be accommodated.
 */
void controller_ensure_capacity(controller_t* controller, size_t new_size);

/**
 * @brief Destroys the controller and frees allocated memory.
 * 
 * @param controller A pointer to the controller to destroy.
 */
void controller_destroy(controller_t* controller);

/**
 * @brief Copies the contents of one controller to another.
 * 
 * @param src A pointer to the source controller.
 * @param dest A pointer to the destination controller.
 */
void controller_copy(const controller_t* src, controller_t* dest);

/**
 * @brief Clears the controller, setting its size to zero.
 * 
 * @param controller A pointer to the controller to clear.
 */
void controller_clear(controller_t* controller);

/**
 * @brief Adds a new car to the controller.
 * 
 * @param controller A pointer to the controller.
 * @param new_car The new car to add.
 */
void controller_push(controller_t* controller, const connectedcar_t* new_car);

/**
 * @brief Removes the last car from the controller.
 * 
 * @param controller A pointer to the controller.
 */
void controller_pop(controller_t* controller);

/**
 * @brief Gets the last car in the controller.
 * 
 * @param controller A pointer to the controller.
 * @return A pointer to the last car, or NULL if the controller is empty.
 */
connectedcar_t* controller_last(const controller_t* controller);

/**
 * @brief Inserts a new car at a specific position in the controller.
 * 
 * @param controller A pointer to the controller.
 * @param pos The position to insert the new car.
 * @param new_car The new car to insert.
 */
void controller_insert_at(controller_t* controller, size_t pos, const connectedcar_t* new_car);

/**
 * @brief Gets the name of the car based on its connection socket value.
 * 
 * @param controller A pointer to the controller.
 * @param connection_socket The connection socket value to search for.
 * @return A pointer to the name of the car, or NULL if not found.
 */
const char* controller_get_name_by_socket(const controller_t* controller, int connection_socket);

/**
 * @brief Gets the connection socket of a car based on its name.
 * 
 * @param controller A pointer to the controller.
 * @param name The name of the car to search for.
 * @return The connection socket of the car, or -1 if not found.
 */
int controller_get_socket_by_name(const controller_t* controller,char name[50]);
/**
 * @brief Removes a car at a specific position in the controller.
 * 
 * @param controller A pointer to the controller.
 * @param pos The position of the car to remove.
 */
void controller_remove_by_name(controller_t* controller, const char* name);

/**
 * @brief Applies a callback function to each car in the controller.
 * 
 * @param controller A pointer to the controller.
 * @param callback The callback function to apply.
 * @param info Additional information to pass to the callback function.
 */
void controller_foreach(controller_t* controller, void (*callback)(connectedcar_t*, void*), void* info);

/**
 * @brief Edits a car's data in the controller based on the provided name and operation code.
 * 
 * @param controller A pointer to the controller.
 * @param name The name of the car to edit.
 * @param op_code The operation code indicating which field to edit.
 * @param new_value The new value to set for the specified field.
 */
void controller_set(controller_t* controller, const char* name, int op_code, const char* new_value);

/**
 * @brief Gets a car's data from the controller based on the provided name and operation code.
 * 
 * @param controller A pointer to the controller.
 * @param name The name of the car to get data from.
 * @param op_code The operation code indicating which field to get.
 * @param out_value A buffer to store the retrieved value.
 * @param buffer_size The size of the output buffer.
 */
void controller_get(const controller_t* controller, const char* name, int op_code, char* out_value, size_t buffer_size);

/**
 * @brief Prints all the information of the controller and its cars.
 * 
 * @param controller A pointer to the controller.
 */
void controller_print(const controller_t* controller);





#endif // CONTROLLERMEMORY_H

