#include <assert.h>
#include "controllermemory.h"

void test_controller_init() {
    controller_t controller;
    controller_init(&controller);
    assert(controller.size == 0);
    assert(controller.capacity == INITIAL_CAPACITY);
    assert(controller.data != NULL);
    controller_destroy(&controller);
}

void test_controller_ensure_capacity() {
    controller_t controller;
    controller_init(&controller);
    size_t initial_capacity = controller.capacity;
    controller_ensure_capacity(&controller, initial_capacity + 1);
    assert(controller.capacity > initial_capacity);
    controller_destroy(&controller);
}

void test_controller_push_and_pop() {
    controller_t controller;
    controller_init(&controller);

    connectedcar_t car = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    controller_push(&controller, &car);
    assert(controller.size == 1);
    assert(strcmp(controller.data[0].name, "Car1") == 0);

    controller_pop(&controller);
    assert(controller.size == 0);

    controller_destroy(&controller);
}

void test_controller_last() {
    controller_t controller;
    controller_init(&controller);

    connectedcar_t car = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    controller_push(&controller, &car);
    connectedcar_t* last_car = controller_last(&controller);
    assert(last_car != NULL);
    assert(strcmp(last_car->name, "Car1") == 0);

    controller_destroy(&controller);
}

void test_controller_insert_at() {
    controller_t controller;
    controller_init(&controller);

    connectedcar_t car1 = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    connectedcar_t car2 = {"Car2", "10", "0", "idle", "0", "5", 2, 1};
    controller_push(&controller, &car1);
    controller_insert_at(&controller, 0, &car2);
    assert(controller.size == 2);
    assert(strcmp(controller.data[0].name, "Car2") == 0);

    controller_destroy(&controller);
}

void test_controller_get_name_by_socket() {
    controller_t controller;
    controller_init(&controller);

    connectedcar_t car = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    controller_push(&controller, &car);
    const char* name = controller_get_name_by_socket(&controller, 1);
    assert(name != NULL);
    assert(strcmp(name, "Car1") == 0);

    controller_destroy(&controller);
}

void test_controller_remove_by_name() {
    controller_t controller;
    controller_init(&controller);

    connectedcar_t car = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    controller_push(&controller, &car);
    controller_remove_by_name(&controller, "Car1");
    assert(controller.size == 0);

    controller_destroy(&controller);
}

void test_controller_set_and_get() {
    controller_t controller;
    controller_init(&controller);

    connectedcar_t car = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    controller_push(&controller, &car);

    controller_set(&controller, "Car1", 1, "15");
    char buffer[50];
    controller_get(&controller, "Car1", 1, buffer, sizeof(buffer));
    assert(strcmp(buffer, "15") == 0);

    controller_destroy(&controller);
}

void test_controller_clear() {
    controller_t controller;
    controller_init(&controller);

    connectedcar_t car = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    controller_push(&controller, &car);
    controller_clear(&controller);
    assert(controller.size == 0);

    controller_destroy(&controller);
}

void test_controller_copy() {
    controller_t src, dest;
    controller_init(&src);
    controller_init(&dest);

    connectedcar_t car = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    controller_push(&src, &car);
    controller_copy(&src, &dest);
    assert(dest.size == 1);
    assert(strcmp(dest.data[0].name, "Car1") == 0);

    controller_destroy(&src);
    controller_destroy(&dest);
}

void callback(connectedcar_t* car, void* info) {
        car->available = 0;
    }


void test_controller_foreach() {
    controller_t controller;
    controller_init(&controller);

    connectedcar_t car1 = {"Car1", "10", "0", "idle", "0", "5", 1, 1};
    connectedcar_t car2 = {"Car2", "10", "0", "idle", "0", "5", 2, 1};
    controller_push(&controller, &car1);
    controller_push(&controller, &car2);

    
    controller_foreach(&controller, callback, NULL);
    assert(controller.data[0].available == 0);
    assert(controller.data[1].available == 0);

    controller_destroy(&controller);
}

int main() {
    test_controller_init();
    test_controller_ensure_capacity();
    test_controller_push_and_pop();
    test_controller_last();
    test_controller_insert_at();
    test_controller_get_name_by_socket();
    test_controller_remove_by_name();
    test_controller_set_and_get();
    test_controller_clear();
    test_controller_copy();
    test_controller_foreach();

    printf("All tests passed!\n");
    return 0;
}