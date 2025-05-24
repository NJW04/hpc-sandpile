# Makefile for the sandpile serial implementation (with a run target)

# Compiler and flags
CC       := gcc
CFLAGS   := -Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include -O3 -Wall -std=c99
LDFLAGS  := -L/opt/homebrew/opt/libomp/lib -lomp
SFLAGS := -O3 -Wall -std=c99
MFLAGS := mpicc -openmpi -mp 
# when running on cluster, exlude the -I and -L flags

# Source, Object, and Executable names
SERIAL_SRC      := sandpile_serial.c
SERIAL_OBJ      := sandpile_serial.o
SERIAL_TARGET   := sandpile_serial

OMP_SRC    := sandpile_omp.c
OMP_OBJ    := sandpile_omp.o
OMP_TARGET    := sandpile_omp

.PHONY: all serial run clean

# Default: build the serial executable
all: serial

# Build the serial executable
serial: $(SERIAL_TARGET)
# Link step
$(SERIAL_TARGET): $(SERIAL_OBJ)
	$(CC) $(SFLAGS) -o $@ $^

# Compile step
$(SERIAL_OBJ): $(SERIAL_SRC)
	$(CC) $(SFLAGS) -c $< -o $@

omp: $(OMP_TARGET)

$(OMP_TARGET): $(OMP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(OMP_OBJ): $(OMP_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Build & run the serial executable
run_serial: serial
	./$(SERIAL_TARGET)

run_omp: omp
	./$(OMP_TARGET)

# Clean up
clean:
	rm -f $(SERIAL_OBJ) $(SERIAL_TARGET) 
	rm -f $(OMP_OBJ) $(OMP_TARGET)