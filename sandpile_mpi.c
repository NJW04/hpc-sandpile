
// paper states that asynchronous and synchronous communication both perform at same time, assuming there are no data races
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <mpi.h>

// default grid size N x M
#ifndef N
#define N 513 /* number of interior rows */
#endif

#ifndef M
#define M 513 /* number of interior columns */
#endif

static inline int sync_compute_new_state(int *sand, int *next, int cols, int y, int x)
/*
 * Computes new sandpile state for cell [y,x]
 *
 * Parameters:
 *   sand - pointer to the current sandpile grid
 *   next - pointer to the grid where the new state is written
 *   cols - total number of columns in the grid (with border)
 *   y    - row index of the cell
 *   x    - column index of the cell
 *
 * Returns:
 *
 */
{
    int idx = y * cols + x; // gets index of cell (y,x) in "1D" array
    next[idx] = sand[idx] % 4 + sand[idx - 1] / 4 + sand[idx + 1] / 4 + sand[idx - cols] / 4 + sand[idx + cols] / 4;
    return next[idx] != sand[idx]; // returns true if changed and then updates next
}

int main(int argc, char *argv[])
{

    int rank, size;
    int changed;
    const int width = M;
    const int height = N;
    MPI_Init(&argc, &argv);               // initialises mpi environment
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // returns ID of calling process
    MPI_Comm_size(MPI_COMM_WORLD, &size); // returns number of processes

    // ---------- Allocate grids in memory ---------- //
    int rowsPerProc = (N / size) + 2; // e.g. 512 / 8 = 64 rows for each processor, plus 2 for top and bottom ghost rows
    int colsPerProc = M + 2;          // real columns plus left and right ghost columns
    int realHeight = N / size;
    int realWidth = M;
    int *sand = malloc(rowsPerProc * colsPerProc * sizeof(int));
    int *next = malloc(rowsPerProc * colsPerProc * sizeof(int));

    for (int y = 1; y <= realHeight; y++)
    {
        for (int x = 1; x <= realWidth; x++)
        {
            sand[y * colsPerProc + x] = 4; // intialise all cells with 4 grains except for the ghost rows
        }
    }

    if (!sand || !next)
    {
        perror("malloc");
        return EXIT_FAILURE;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    // individual process communication can cause deadlocks
    // MPI_Send(address, count, datatype, destination, tag, comm)
    // MPI_Recv(address, maxcount, datatype, source, tag, comm, status)
    // try to avoid saving a new grid in every iteration (save part of it) -> high computation cost
    // bottleneck is communication between processes so look into that

    // ---------- Relaxation loop ---------- //
    bool entireGridChanged = true;
    while (entireGridChanged)
    {

        MPI_Request requests[4];
        int requestCount = 0;

        // exchange ghost rows with neighbouring processes
        // ensures current process sends first real row to previous process and receives into top ghost row from previous process
        //  Let's say process 3 is running: process 3 will send 1st row to process 2 and receive into its top ghost row from process 2

        if (rank > 0) // if not the first process
        {
            MPI_Irecv(&sand[0 * colsPerProc], colsPerProc, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, &requests[requestCount++]); // top ghost row
        }
        if (rank < size - 1)
        {
            MPI_Irecv(&sand[(realHeight + 1) * colsPerProc], colsPerProc, MPI_INT, rank + 1, 1, MPI_COMM_WORLD, &requests[requestCount++]); // bottom ghost row
        }

        // post non-blocking sends
        if (rank > 0)
        {
            MPI_Isend(&sand[1 * colsPerProc], colsPerProc, MPI_INT, rank - 1, 1, MPI_COMM_WORLD, &requests[requestCount++]); // first real row
        }
        if (rank < size - 1)
        {
            MPI_Isend(&sand[realHeight * colsPerProc], colsPerProc, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, &requests[requestCount++]); // last real row
        }

        MPI_Waitall(requestCount, requests, MPI_STATUSES_IGNORE);

        changed = false;
        for (int y = 1; y <= realHeight; y++)
        {
            for (int x = 1; x <= realWidth; x++)
            {
                changed |= sync_compute_new_state(sand, next, colsPerProc, y, x); // as soon as any cell changes, we set changed to true
            }
        }
        /* swap buffers */
        int *tmp = sand;
        sand = next;
        next = tmp;

        MPI_Allreduce(&changed, &entireGridChanged, 1, MPI_C_BOOL, MPI_LOR, MPI_COMM_WORLD); // if any process's local grid has changed then change entireGridChanged to true
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double processorTime = MPI_Wtime() - start;
    double maxProcTime = 0.0;
    MPI_Reduce(&processorTime, &maxProcTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    int *numPixels = NULL;
    int *finalImageData = NULL;
    int *offsets = NULL;

    // allocate space in memory
    if (rank == 0)
    {
        numPixels = malloc(size * sizeof(int)); // array to store the amount of pixel data from each process
        offsets = malloc(size * sizeof(int));   // array to know where to place each process's data in array
    }

    int localProcPixels = realHeight * realWidth;                                       // essentially N x M, for number of interior pixels
    MPI_Gather(&localProcPixels, 1, MPI_INT, numPixels, 1, MPI_INT, 0, MPI_COMM_WORLD); // gather

    // gather all image output from processors and place into first processor
    if (rank == 0)
    {
        int totalPixels = 0;
        offsets[0] = 0;
        for (int i = 0; i < size; i++)
        {
            totalPixels += numPixels[i];
            if (i > 0)
                offsets[i] = offsets[i - 1] + numPixels[i - 1];
        }
        finalImageData = malloc(totalPixels * sizeof(int));
    }

    int *sendLocalData = malloc(localProcPixels * sizeof(int));
    for (int y = 1; y <= realHeight; y++)
        for (int x = 1; x <= width; x++)
            sendLocalData[(y - 1) * width + (x - 1)] = sand[y * colsPerProc + x];

    MPI_Gatherv(sendLocalData, localProcPixels, MPI_INT,
                finalImageData, numPixels, offsets, MPI_INT, 0, MPI_COMM_WORLD);

    MPI_Finalize(); // terminates mpi and cleanup

    if (rank == 0)
    {
        /* Write P6 PPM */
        FILE *fp = fopen("sandpile.ppm", "wb");
        if (!fp)
        {
            perror("fopen");
            return EXIT_FAILURE;
        }
        fprintf(fp, "P6\n%d %d\n255\n", width, height);

        for (int i = 0; i < height * width; i++)
        {
            unsigned char r, g, b;
            switch (
                finalImageData[i])
            {
            case 0:
                r = 0;
                g = 0;
                b = 0;
                break; // black
            case 1:
                r = 0;
                g = 255;
                b = 0;
                break; // green
            case 2:
                r = 0;
                g = 0;
                b = 255;
                break; // blue
            case 3:
                r = 255;
                g = 0;
                b = 0;
                break; // red
            default:
                /* should not happen in final stable state */
                r = g = b = 0;
            }
            fputc(r, fp);
            fputc(g, fp);
            fputc(b, fp);
        }
        fclose(fp);
        fprintf(stderr, "Wrote sandpile.ppm (%dx%d)\n", width, height);
        fprintf(stderr, "Ran in (%f) seconds\n", maxProcTime);
    }

    free(sand);
    free(next);

    free(sendLocalData);
    if (rank == 0)
    {
        free(finalImageData);
        free(numPixels);
        free(offsets);
    }
    return EXIT_SUCCESS;
}
