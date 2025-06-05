#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#ifndef N
#define N 513
#endif

#ifndef M
#define M 513
#endif

static inline int sync_compute_new_state(int *sand, int *next, int cols, int y, int x)
{
    int idx = y * cols + x;
    next[idx] = sand[idx] % 4 + sand[idx - 1] / 4 + sand[idx + 1] / 4 + sand[idx - cols] / 4 + sand[idx + cols] / 4;
    return next[idx] != sand[idx];
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int width = M;
    const int global_height = N;
    const int cols = width + 2;

    int local_height = global_height / size;
    int rem = global_height % size;

    if (rank < rem)
        local_height++;

    int local_rows = local_height + 2;

    int *sand = calloc(local_rows * cols, sizeof(int));
    int *next = calloc(local_rows * cols, sizeof(int));

    if (!sand || !next)
    {
        perror("malloc");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    // Initialize sandpile (4 grains per cell)
    for (int y = 1; y <= local_height; y++)
        for (int x = 1; x <= width; x++)
            sand[y * cols + x] = 4;

    clock_t start = clock();

    bool global_changed = true;
    while (global_changed)
    {
        // Exchange ghost rows
        if (rank > 0)
        {
            MPI_Sendrecv(&sand[1 * cols], cols, MPI_INT, rank - 1, 0,
                         &sand[0 * cols], cols, MPI_INT, rank - 1, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        // individual process communication can cause deadlocks
        // MPI_Send(address, count, datatype, destination, tag, comm)
        // MPI_Recv(address, maxcount, datatype, source, tag, comm, status)

        if (rank < size - 1)
        {
            MPI_Sendrecv(&sand[local_height * cols], cols, MPI_INT, rank + 1, 0,
                         &sand[(local_height + 1) * cols], cols, MPI_INT, rank + 1, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        bool local_changed = false;
        for (int y = 1; y <= local_height; y++)
        {
            for (int x = 1; x <= width; x++)
            {
                local_changed |= sync_compute_new_state(sand, next, cols, y, x);
            }
        }

        // Swap buffers
        int *tmp = sand;
        sand = next;
        next = tmp;

        MPI_Allreduce(&local_changed, &global_changed, 1, MPI_C_BOOL, MPI_LOR, MPI_COMM_WORLD);
    }

    clock_t end = clock();
    double local_time = (double)(end - start) / CLOCKS_PER_SEC;
    double max_time;
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Gather results to root for output
    int *recvcounts = NULL, *displs = NULL;
    int *final_data = NULL;

    if (rank == 0)
    {
        recvcounts = malloc(size * sizeof(int));
        displs = malloc(size * sizeof(int));
    }

    int local_pixels = local_height * width;

    MPI_Gather(&local_pixels, 1, MPI_INT, recvcounts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        int total_pixels = 0;
        displs[0] = 0;
        for (int i = 0; i < size; i++)
        {
            total_pixels += recvcounts[i];
            if (i > 0)
                displs[i] = displs[i - 1] + recvcounts[i - 1];
        }
        final_data = malloc(total_pixels * sizeof(int));
    }

    int *sendbuf = malloc(local_pixels * sizeof(int));
    for (int y = 1; y <= local_height; y++)
        for (int x = 1; x <= width; x++)
            sendbuf[(y - 1) * width + (x - 1)] = sand[y * cols + x];

    MPI_Gatherv(sendbuf, local_pixels, MPI_INT,
                final_data, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0)
    {
        FILE *fp = fopen("sandpile.ppm", "wb");
        fprintf(fp, "P6\n%d %d\n255\n", width, global_height);

        for (int i = 0; i < global_height * width; i++)
        {
            unsigned char r, g, b;
            switch (final_data[i])
            {
            case 0:
                r = g = b = 0;
                break;
            case 1:
                r = 0;
                g = 255;
                b = 0;
                break;
            case 2:
                r = 0;
                g = 0;
                b = 255;
                break;
            case 3:
                r = 255;
                g = 0;
                b = 0;
                break;
            default:
                r = g = b = 0;
                break;
            }
            fputc(r, fp);
            fputc(g, fp);
            fputc(b, fp);
        }
        fclose(fp);
        fprintf(stderr, "Wrote sandpile.ppm (%dx%d)\n", width, global_height);
        fprintf(stderr, "Max runtime across processes: %.3f seconds\n", max_time);
    }

    free(sand);
    free(next);
    free(sendbuf);
    if (rank == 0)
    {
        free(final_data);
        free(recvcounts);
        free(displs);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
