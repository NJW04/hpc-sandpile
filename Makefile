# Makefile for the sandpile serial implementation (with a run target)

# Compiler and flags
CC       := gcc
CFLAGS   := -Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include -O3 -Wall -std=c99
LDFLAGS  := -L/opt/homebrew/opt/libomp/lib -lomp
SFLAGS := -O3 -Wall -std=c99
MFLAGS := mpicc -std=c99 -O3 -Wall
# when running on cluster, exlude the -I and -L flags

# Source, Object, and Executable names
SERIAL_SRC      := sandpile_serial.c
SERIAL_OBJ      := sandpile_serial.o
SERIAL_TARGET   := sandpile_serial

OMP_SRC    := sandpile_omp.c
OMP_OBJ    := sandpile_omp.o
OMP_TARGET    := sandpile_omp

MPI_SRC    := sandpile_mpi.c
MPI_OBJ    := sandpile_mpi.o
MPI_TARGET    := sandpile_mpi

# MPI_SRC    := test.c
#  MPI_OBJ    := test.o
#   MPI_TARGET    := test

.PHONY: all serial run_serial run_omp run_mpi clean 

# Default: build the serial executable
all: batchRun.py

# Build the serial executable
serial: $(SERIAL_TARGET)
# Link step
$(SERIAL_TARGET): $(SERIAL_OBJ)
	$(CC) $(SFLAGS) -o $@ $^
#compile step
$(SERIAL_OBJ): $(SERIAL_SRC)
	$(CC) $(SFLAGS) -c $< -o $@

omp: $(OMP_TARGET)

$(OMP_TARGET): $(OMP_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(OMP_OBJ): $(OMP_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

mpi: $(MPI_TARGET)

$(MPI_TARGET): $(MPI_OBJ)
	$(MFLAGS) -o $@ $^

$(MPI_OBJ): $(MPI_SRC)
	$(MFLAGS) -c $< -o $@

# Build & run the serial executable
run_serial: serial
	./$(SERIAL_TARGET)

run_omp: omp
	./$(OMP_TARGET)

run_mpi: mpi
	mpiexec -np $(shell sysctl -n hw.ncpu) ./$(MPI_TARGET)
# $(sysctl -n hw.ncpu) is for macos

# Clean up
clean:
	rm -f $(SERIAL_OBJ) $(SERIAL_TARGET) 
	rm -f $(OMP_OBJ) $(OMP_TARGET)
	rm -f $(MPI_OBJ) $(MPI_TARGET)