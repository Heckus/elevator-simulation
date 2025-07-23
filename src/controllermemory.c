
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "sharedmemory.h"
#include "controllermemory.h"

#define INITIAL_CAPACITY 10
#define GROWTH_FACTOR 2
#define MAX_QUEUE_SIZE 50

void queue_init(connectedcar_t* car) {
    car->queue_head = NULL;
    car->current_direction = DIRECTION_IDLE;
    car->elevator_status = stringToStatus(car->status);
}

bool can_service_request(connectedcar_t* car, int source_floor, int dest_floor) {
    if (car == NULL || car->highest_floor == NULL || car->lowest_floor == NULL) {
        return false;
    }
    printf("source floor: %s\n", car->highest_floor);   
    printf("dest floor: %s\n", car->lowest_floor);

    int highest = stringToFloor(car->highest_floor);
    int lowest = stringToFloor(car->lowest_floor);
    return source_floor >= lowest && source_floor <= highest &&
           dest_floor >= lowest && dest_floor <= highest;
}



bool add_to_car_queue(connectedcar_t* car, int source_floor, int dest_floor) {
    Direction request_direction = (dest_floor > source_floor) ? DIRECTION_UP : DIRECTION_DOWN;
    int current_floor = stringToFloor(car->currentfloor);
    
    // Create new nodes
    QueueNode* source_node = malloc(sizeof(QueueNode));
    QueueNode* dest_node = malloc(sizeof(QueueNode));
    if (!source_node || !dest_node) {
        free(source_node);
        free(dest_node);
        return false;
    }
    
    source_node->floor = source_floor;
    source_node->direction = request_direction;
    dest_node->floor = dest_floor;
    dest_node->direction = request_direction;
    
    // Empty queue case
    if (!car->queue_head) {
        source_node->next = dest_node;
        dest_node->next = NULL;
        car->queue_head = source_node;
        return true;
    }
    
    // Find appropriate position in queue based on direction blocks
    QueueNode* current = car->queue_head;
    QueueNode* prev = NULL;
    Direction current_block_direction = car->current_direction;
    
    // Special case: if current floor equals source floor
    if (current_floor == source_floor && 
        car->elevator_status != Closing) {
        // Insert at beginning if going in same direction
        if (car->current_direction == request_direction || 
            car->current_direction == DIRECTION_IDLE) {
            source_node->next = car->queue_head;
            dest_node->next = source_node->next;
            car->queue_head = source_node;
            return true;
        }
    }
    
    // Find appropriate block for insertion
    while (current != NULL) {
        // Check if we can insert in current direction block
        if (current_block_direction == request_direction) {
            if ((request_direction == DIRECTION_UP && 
                 source_floor >= current->floor) ||
                (request_direction == DIRECTION_DOWN && 
                 source_floor <= current->floor)) {
                // Insert here
                source_node->next = current->next;
                dest_node->next = source_node->next;
                current->next = source_node;
                return true;
            }
        }
        
        // Check for direction change
        if (current->next && 
            current->direction != current->next->direction) {
            current_block_direction = current->next->direction;
        }
        
        prev = current;
        current = current->next;
    }
    
    // If no suitable position found, add to end
    if (prev) {
        prev->next = source_node;
        source_node->next = dest_node;
        dest_node->next = NULL;
    }
    
    return true;
}


// Function to get next destination floor for a car
int get_next_destination(connectedcar_t* car) {
    if (!car->queue_head) return -1;
    return car->queue_head->floor;
}

// Function to remove current floor from queue when reached
void remove_from_car_queue(connectedcar_t* car) {
    if (!car->queue_head) return;
    
    QueueNode* temp = car->queue_head;
    car->queue_head = car->queue_head->next;
    free(temp);
}





/**
 * @brief Initializes the controller with an initial capacity.
 * 
 * @param controller A pointer to the controller to initialize.
 */
void controller_init(controller_t* controller) {
    if (controller == NULL) return;
    controller->capacity = INITIAL_CAPACITY;
    controller->size = 0;
    controller->data = (connectedcar_t*)malloc(controller->capacity * sizeof(connectedcar_t));
    if (controller->data == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    // Initialize queues for all cars
    for (size_t i = 0; i < controller->capacity; i++) {
        queue_init(&controller->data[i]);
    }
}

/**
 * @brief Ensures that the controller has enough capacity to store new data.
 * 
 * @param controller A pointer to the controller.
 * @param new_size The new size that needs to be accommodated.
 */
void controller_ensure_capacity(controller_t* controller, size_t new_size) {
    if (controller == NULL || controller->data == NULL) return;
    if (new_size > controller->capacity) {
        size_t new_capacity = (controller->capacity * GROWTH_FACTOR > new_size) ? controller->capacity * GROWTH_FACTOR : new_size;
        connectedcar_t* new_data = (connectedcar_t*)realloc(controller->data, new_capacity * sizeof(connectedcar_t));
        if (new_data == NULL) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        controller->data = new_data;
        controller->capacity = new_capacity;
    }
}

/**
 * @brief Destroys the controller and frees allocated memory.
 * 
 * @param controller A pointer to the controller to destroy.
 */
void controller_destroy(controller_t* controller) {
    if (controller == NULL) return;
    free(controller->data);
    controller->data = NULL;
    controller->size = 0;
    controller->capacity = 0;
}

/**
 * @brief Copies the contents of one controller to another.
 * 
 * @param src A pointer to the source controller.
 * @param dest A pointer to the destination controller.
 */
void controller_copy(const controller_t* src, controller_t* dest) {
    if (src == NULL || dest == NULL) return;
    dest->size = src->size;
    controller_ensure_capacity(dest, src->size);
    for (size_t i = 0; i < src->size; ++i) {
        dest->data[i] = src->data[i];
    }
}

/**
 * @brief Clears the controller, setting its size to zero.
 * 
 * @param controller A pointer to the controller to clear.
 */
void controller_clear(controller_t* controller) {
    if (controller == NULL) return;
    controller->size = 0;
}

/**
 * @brief Adds a new car to the controller.
 * 
 * @param controller A pointer to the controller.
 * @param new_car The new car to add.
 */
void controller_push(controller_t* controller, const connectedcar_t* new_car) {
    if (controller == NULL || new_car == NULL) return;
    controller_ensure_capacity(controller, controller->size + 1);
    controller->data[controller->size] = *new_car;
    controller->size++;
}

/**
 * @brief Removes the last car from the controller.
 * 
 * @param controller A pointer to the controller.
 */
void controller_pop(controller_t* controller) {
    if (controller == NULL || controller->size == 0) return;
    controller->size--;
}

/**
 * @brief Gets the last car in the controller.
 * 
 * @param controller A pointer to the controller.
 * @return A pointer to the last car, or NULL if the controller is empty.
 */
connectedcar_t* controller_last(const controller_t* controller) {
    if (controller == NULL || controller->size == 0) return NULL;
    return &controller->data[controller->size - 1];
}

/**
 * @brief Inserts a new car at a specific position in the controller.
 * 
 * @param controller A pointer to the controller.
 * @param pos The position to insert the new car.
 * @param new_car The new car to insert.
 */
void controller_insert_at(controller_t* controller, size_t pos, const connectedcar_t* new_car) {
    if (controller == NULL || new_car == NULL) return;
    if (pos > controller->size) {
        pos = controller->size;
    }
    controller_ensure_capacity(controller, controller->size + 1);
    for (size_t i = controller->size; i > pos; --i) {
        controller->data[i] = controller->data[i - 1];
    }
    controller->data[pos] = *new_car;
    controller->size++;
}

/**
 * @brief Gets the name of the car based on its connection socket value.
 * 
 * @param controller A pointer to the controller.
 * @param connection_socket The connection socket value to search for.
 * @return A pointer to the name of the car, or NULL if not found.
 */
const char* controller_get_name_by_socket(const controller_t* controller, int connection_socket) {
    if (controller == NULL) return NULL;
    for (size_t i = 0; i < controller->size; ++i) {
        if (controller->data[i].connectionsocket == connection_socket) {
            return controller->data[i].name;
        }
    }
    return NULL;
}

/**
 * @brief Gets the connection socket of a car based on its name.
 * 
 * @param controller A pointer to the controller.
 * @param name The name of the car to search for.
 * @return The connection socket of the car, or -1 if not found.
 */
int controller_get_socket_by_name(const controller_t* controller,char name[50]) {
    if (controller == NULL || name == NULL) {
        fprintf(stderr, "Error: controller or name is NULL\n");
        return -1;
    }

    for (size_t i = 0; i < controller->size; ++i) {
        if (controller->data[i].name != NULL && strcmp(controller->data[i].name, name) == 0) {
            return controller->data[i].connectionsocket;
        }
    }

    fprintf(stderr, "Error: Car with name %s not found\n", name);
    return -1;
}

/**
 * @brief Removes a car at a specific position in the controller.
 * 
 * @param controller A pointer to the controller.
 * @param pos The position of the car to remove.
 */
void controller_remove_by_name(controller_t* controller, const char* name) {
    if (controller == NULL || name == NULL) return;
    for (size_t i = 0; i < controller->size; ++i) {
        if (strcmp(controller->data[i].name, name) == 0) {
            for (size_t j = i; j < controller->size - 1; ++j) {
                controller->data[j] = controller->data[j + 1];
            }
            controller->size--;
            return;
        }
    }
}

/**
 * @brief Applies a callback function to each car in the controller.
 * 
 * @param controller A pointer to the controller.
 * @param callback The callback function to apply.
 * @param info Additional information to pass to the callback function.
 */
void controller_foreach(controller_t* controller, void (*callback)(connectedcar_t*, void*), void* info) {
    if (controller == NULL || callback == NULL) return;
    for (size_t i = 0; i < controller->size; ++i) {
        callback(&controller->data[i], info);
    }
}

/**
 * @brief Edits a car's data in the controller based on the provided name and operation code.
 * 
 * @param controller A pointer to the controller.
 * @param name The name of the car to edit.
 * @param op_code The operation code indicating which field to edit.
 * @param new_value The new value to set for the specified field.
 */
void controller_set(controller_t* controller, const char* name, int op_code, const char* new_value) {
    if (controller == NULL || name == NULL || new_value == NULL) return;
    for (size_t i = 0; i < controller->size; ++i) {
        if (strcmp(controller->data[i].name, name) == 0) {
            switch (op_code) {
                case 1:
                    strncpy(controller->data[i].highest_floor, new_value, sizeof(controller->data[i].highest_floor) - 1);
                    controller->data[i].highest_floor[sizeof(controller->data[i].highest_floor) - 1] = '\0';
                    break;
                case 2:
                    strncpy(controller->data[i].lowest_floor, new_value, sizeof(controller->data[i].lowest_floor) - 1);
                    controller->data[i].lowest_floor[sizeof(controller->data[i].lowest_floor) - 1] = '\0';
                    break;
                case 3:
                    strncpy(controller->data[i].status, new_value, sizeof(controller->data[i].status) - 1);
                    controller->data[i].status[sizeof(controller->data[i].status) - 1] = '\0';
                    break;
                case 7:
                    strncpy(controller->data[i].previous_status, new_value, sizeof(controller->data[i].previous_status) - 1);
                    controller->data[i].status[sizeof(controller->data[i].previous_status) - 1] = '\0';
                    break;
                case 4:
                    strncpy(controller->data[i].currentfloor, new_value, sizeof(controller->data[i].currentfloor) - 1);
                    controller->data[i].currentfloor[sizeof(controller->data[i].currentfloor) - 1] = '\0';
                    break;
                case 5:
                    strncpy(controller->data[i].destinationfloor, new_value, sizeof(controller->data[i].destinationfloor) - 1);
                    controller->data[i].destinationfloor[sizeof(controller->data[i].destinationfloor) - 1] = '\0';
                    break;
                case 6:
                    controller->data[i].connectionsocket = atoi(new_value);
                    break;
                default:
                    printf("Invalid operation code.\n");
                    break;
            }
            break;
        }
    }
}

/**
 * @brief Gets a car's data from the controller based on the provided name and operation code.
 * 
 * @param controller A pointer to the controller.
 * @param name The name of the car to get data from.
 * @param op_code The operation code indicating which field to get.
 * @param out_value A buffer to store the retrieved value.
 * @param buffer_size The size of the output buffer.
 */
void controller_get(const controller_t* controller, const char* name, int op_code, char* out_value, size_t buffer_size) {
    if (controller == NULL || name == NULL || out_value == NULL) return;
    for (size_t i = 0; i < controller->size; ++i) {
        if (strcmp(controller->data[i].name, name) == 0) {
            switch (op_code) {
                case 1:
                    strncpy(out_value, controller->data[i].highest_floor, buffer_size - 1);
                    out_value[buffer_size - 1] = '\0';
                    break;
                case 2:
                    strncpy(out_value, controller->data[i].lowest_floor, buffer_size - 1);
                    out_value[buffer_size - 1] = '\0';
                    break;
                case 3:
                    strncpy(out_value, controller->data[i].status, buffer_size - 1);
                    out_value[buffer_size - 1] = '\0';
                    break;
                case 4:
                    strncpy(out_value, controller->data[i].currentfloor, buffer_size - 1);
                    out_value[buffer_size - 1] = '\0';
                    break;
                case 5:
                    strncpy(out_value, controller->data[i].destinationfloor, buffer_size - 1);
                    out_value[buffer_size - 1] = '\0';
                    break;
                case 6:
                    snprintf(out_value, buffer_size, "%d", controller->data[i].connectionsocket);
                    break;
                default:
                    printf("Invalid operation code.\n");
                    break;
            }
            break;
        }
    }
}

/**
 * @brief Prints all the information of the controller and its cars.
 * 
 * @param controller A pointer to the controller.
 */
void controller_print(const controller_t* controller) {
    if (controller == NULL) return;
    printf("=======================\n");
    printf("Controller Info:\n");
    printf("=======================\n");
    printf("Size: %zu\n", controller->size);
    printf("Capacity: %zu\n", controller->capacity);
    printf("Cars:\n");
    for (size_t i = 0; i < controller->size; ++i) {
        connectedcar_t* car = &controller->data[i];
        printf("Car %zu:\n", i + 1);
        printf("-----------------------\n");
        printf("Name: %s\n", car->name);
        printf("Top Floor: %s\n", car->highest_floor);
        printf("Bottom Floor: %s\n", car->lowest_floor);
        printf("Status: %s\n", car->status);
        printf("Current Floor: %s\n", car->currentfloor);
        printf("Destination Floor: %s\n", car->destinationfloor);
        printf("Connection Status: %d\n", car->connectionsocket);
        printf("-----------------------\n");
    }
    
}







