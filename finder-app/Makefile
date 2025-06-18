# Name of the output binary
TARGET = writer

# Source files
SRC = writer.c

# Object files
OBJ = $(SRC:.c=.o)

# Compiler and flags
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Werror -g

# Default target
all: $(TARGET)

# Build rule for the target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Clean target to remove binary and object files
clean:
	rm -f $(TARGET) $(OBJ)

# Pattern rule to compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean

