/*
 * sandpile_serial.c
 *
 * Serial (single‐threaded) implementation of the 2D Abelian sandpile model
 * with PPM output coloured by final state:
 *   0→black, 1→green, 2→blue, 3→red
 *
 * Compile with:
 *   gcc -DN=512 -DM=512 -std=c99 -O3 -Wall -o sandpile_serial sandpile_serial.c
 */
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
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // returns ID of calling process
    MPI_Comm_size(MPI_COMM_WORLD, &size); // returns number of processes

    const int height = N;
    const int width = M;
    const int rows = height + 2; // +2 for sink border
    const int cols = width + 2;  // +2 for sink border
    /* use of sink cells: absorb excess sand*/

    /* Allocate grids in memory */
    int *sand = malloc(rows * cols * sizeof(int));
    int *next = malloc(rows * cols * sizeof(int));
    if (!sand || !next)
    {
        perror("malloc");
        return EXIT_FAILURE;
    }

    /* Initialise both grids by making each cell "empty" */
    for (int i = 0; i < rows * cols; i++)
    {
        sand[i] = next[i] = 0;
    }

    /* Makes entire sandpile unstable with each cell containing 4 grains */
    // try to avoid saving a new grid in every iteration (save part of it) -> high computation cost
    // bottleneck is communication between processes so look into that
    // maybe look into asynchronous communication!
    for (int y = 1; y <= height; y++)
        for (int x = 1; x <= width; x++)
            sand[y * cols + x] = 4;

    /* Alternatively, to match Figure 1: one centre cell = width*height, rest = 0:
    int cy = height/2 + 1, cx = width/2 + 1;
    sand[cy*cols + cx] = width * height;
    */

    /* Relaxation loop */
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (int y = 1; y <= height; y++)
        {
            for (int x = 1; x <= width; x++)
            {
                changed |= sync_compute_new_state(sand, next, cols, y, x); // as soon as any cell changes, we set changed to true
            }
        }
        /* swap buffers */
        int *tmp = sand;
        sand = next;
        next = tmp;
    }

    MPI_Finalize();

    /* Write P6 PPM */
    FILE *fp = fopen("sandpile.ppm", "wb");
    if (!fp)
    {
        perror("fopen");
        return EXIT_FAILURE;
    }
    fprintf(fp, "P6\n%d %d\n255\n", width, height);

    for (int y = 1; y <= height; y++)
    {
        for (int x = 1; x <= width; x++)
        {
            unsigned char r, g, b;
            int v = sand[y * cols + x];
            switch (v)
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
    }
    fclose(fp);
    fprintf(stderr, "Wrote sandpile.ppm (%dx%d)\n", width, height);

    free(sand);
    free(next);
    return EXIT_SUCCESS;
}
