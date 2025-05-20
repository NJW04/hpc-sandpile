# Makefile for the sandpile serial implementation (with a run target)

# Compiler and flags
CC       := gcc
CFLAGS   := -O3 -Wall -std=c99

# Source, Object, and Executable names
SRC      := sandpile_serial.c
OBJ      := sandpile_serial.o
TARGET   := sandpile_serial

.PHONY: all serial run clean

# Default: build the serial executable
all: serial

# Build the serial executable
serial: $(TARGET)

# Link step
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile step
$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Build & run the serial executable
run: serial
	./$(TARGET)

# Clean up
clean:
	rm -f $(OBJ) $(TARGET)
