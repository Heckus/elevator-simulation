# Makefile for CAB403VM Assessed3

# Compiler and flags
CC = gcc
CFLAGS = -g -Wall -Wextra -lrt -pthread

# Source files
SRCS = car.c controller.c call.c internal.c safety.c sharedmemory.c controllermemory.c

# Header files
HDRS = sharedmemory.h controllermemory.h

# Default target
all: car controller call internal safety

# Object files
OBJS = $(SRCS:.c=.o)

# Update object file rules
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Update targets to use object files
car: car.o sharedmemory.o 
	$(CC) $(CFLAGS) -o car car.c sharedmemory.c 

controller: controller.o  controllermemory.o sharedmemory.o
	$(CC) $(CFLAGS) -o  controller controller.c  controllermemory.o sharedmemory.o

call: call.o sharedmemory.o 
	$(CC) $(CFLAGS) -o call call.c sharedmemory.o 

internal: internal.o sharedmemory.o 
	$(CC) $(CFLAGS) -o internal internal.c sharedmemory.o 

safety: safety.o sharedmemory.o 
	$(CC) $(CFLAGS) -o safety safety.c sharedmemory.o 

# Clean target (optional)	
clean:
	rm -f *.o car controller call internal safety

.PHONY: all car controller call internal safety clean

# Usage notes
help:
	@echo "Usage: make [target]"
	@echo "Targets:"
	@echo "  all        - Build all components"
	@echo "  car        - Build the elevator car component"
	@echo "  controller - Build the control system component"
	@echo "  call       - Build the call component"
	@echo "  internal   - Build the internal component"
	@echo "  safety     - Build the safety component"
	@echo "  clean      - Remove all compiled files"