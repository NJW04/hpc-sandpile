/*
 * sandpile_openmp.c
 *
 * Parallel (shared-memory using OpenMP) implementation of the 2D Abelian
 * sandpile model with PPM output coloured by final state:
 *   0→black, 1→green, 2→blue, 3→red
 *
 * Also measures and reports the runtime of the relaxation phase.
 *
 * Compile with:
 *   gcc -fopenmp -DN=512 -DM=512 -std=c99 -O3 -Wall -o sandpile_openmp sandpile_openmp.c
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <stdbool.h>
 #include <time.h>
 #include <omp.h>
 
 #ifndef N
 #define N 512   /* number of interior rows */
 #endif
 
 #ifndef M
 #define M 512   /* number of interior columns */
 #endif
 
 /**
  * sync_compute_new_state
  * ----------------------
  * Compute the next state of a single cell (y, x) in the sandpile grid
  * using a synchronous update. It sums the remainder of the current cell
  * modulo 4 plus one quarter of each of its four neighbors. Writes the
  * result into the 'next' grid and returns 1 if the cell value changed,
  * 0 otherwise.
  */
 static inline int sync_compute_new_state(int *sand, int *next, int cols, int y, int x) {
     int idx = y * cols + x;
     next[idx] = sand[idx] % 4
               + sand[idx - 1]    / 4  /* left neighbor */
               + sand[idx + 1]    / 4  /* right neighbor */
               + sand[idx - cols] / 4  /* above neighbor */
               + sand[idx + cols] / 4; /* below neighbor */
     return next[idx] != sand[idx];
 }
 
 int main(int argc, char *argv[]) {
     const int height = N;
     const int width  = M;
     const int rows = height + 2;  /* include sink border */
     const int cols = width  + 2;
 
     /* Allocate grids */
     int *sand = malloc(rows * cols * sizeof(int));
     int *next = malloc(rows * cols * sizeof(int));
     if (!sand || !next) {
         perror("malloc");
         return EXIT_FAILURE;
     }
 
     /* Initialize to zero */
     #pragma omp parallel for
     for (int i = 0; i < rows * cols; i++) {
         sand[i] = next[i] = 0;
     }
 
     /* Set every interior cell to 4 grains (unstable start) */
     #pragma omp parallel for collapse(2)
     for (int y = 1; y <= height; y++) {
         for (int x = 1; x <= width; x++) {
             sand[y * cols + x] = 4;
         }
     }
 
     /* Measure relaxation runtime */
     struct timespec t_start, t_end;
     clock_gettime(CLOCK_MONOTONIC, &t_start);
 
     /* Relaxation: repeat until no cell changes */
     bool changed = true;
     while (changed) {
         int changed_int = 0;
         /* Parallel sweep of interior cells */
         #pragma omp parallel for collapse(2) reduction(|:changed_int)
         for (int y = 1; y <= height; y++) {
             for (int x = 1; x <= width; x++) {
                 changed_int |= sync_compute_new_state(sand, next, cols, y, x);
             }
         }
         changed = changed_int;
         /* Swap buffers */
         int *tmp = sand;
         sand = next;
         next = tmp;
     }
 
     clock_gettime(CLOCK_MONOTONIC, &t_end);
     double elapsed = (t_end.tv_sec - t_start.tv_sec)
                     + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
     fprintf(stderr, "[OpenMP] Relaxation runtime: %.6f seconds\n", elapsed);
 
     /* Write P6 PPM */
     FILE *fp = fopen("sandpile_openmp.ppm", "wb");
     if (!fp) {
         perror("fopen");
         return EXIT_FAILURE;
     }
     fprintf(fp, "P6\n%d %d\n255\n", width, height);
     for (int y = 1; y <= height; y++) {
         for (int x = 1; x <= width; x++) {
             int v = sand[y * cols + x];
             unsigned char r, g, b;
             switch (v) {
                 case 0: r = 0;   g = 0;   b = 0;   break;
                 case 1: r = 0;   g = 255; b = 0;   break;
                 case 2: r = 0;   g = 0;   b = 255; break;
                 case 3: r = 255; g = 0;   b = 0;   break;
                 default: r = g = b = 0;
             }
             fputc(r, fp);
             fputc(g, fp);
             fputc(b, fp);
         }
     }
     fclose(fp);
     fprintf(stderr, "Wrote sandpile_openmp.ppm (%dx%d)\n", width, height);
 
     free(sand);
     free(next);
     return EXIT_SUCCESS;
 }