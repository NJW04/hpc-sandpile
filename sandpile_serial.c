/*
 * sandpile_serial.c
 *
 * Serial (single-threaded) implementation of the 2D Abelian sandpile model
 * with PPM output coloured by final state:
 *   0→black, 1→green, 2→blue, 3→red
 *
 * Also measures and reports the runtime of the relaxation phase.
 *
 * Compile with:
 *   gcc -DN=512 -DM=512 -std=c99 -O3 -Wall -o sandpile_serial sandpile_serial.c
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <stdbool.h>
 #include <time.h>
 
 #ifndef N
 //#define N 512   /* number of interior rows */
 #define N 512
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
 
 /**
  * main
  * ----
  * Entry point for the serial sandpile simulation. Sets up the grid,
  * initializes cell values, measures the relaxation runtime, and writes
  * the final configuration to a PPM image file.
  */
 int main(int argc, char *argv[]) {
     const int height = N;
     const int width  = M;
     const int rows = height + 2;  /* include sink border */
     const int cols = width  + 2;
 
     /* Allocate two grids: current (sand) and next state (next) */
     int *sand = malloc(rows * cols * sizeof(int));
     int *next = malloc(rows * cols * sizeof(int));
     if (!sand || !next) {
         perror("malloc");
         return EXIT_FAILURE;
     }
 
     /* Initialize all cells (including border) to zero */
     for (int i = 0; i < rows * cols; i++) {
         sand[i] = next[i] = 0;
     }
 
     /* Set every interior cell to 4 grains (unstable start) */
     for (int y = 1; y <= height; y++) {
         for (int x = 1; x <= width; x++) {
             sand[y * cols + x] = 4;
         }
     }
 
     /* Alternative initialization: single huge centre pile
     int cy = height/2 + 1, cx = width/2 + 1;
     sand[cy * cols + cx] = width * height;
     */
 
     /* Measure relaxation runtime */
     struct timespec t_start, t_end;
     clock_gettime(CLOCK_MONOTONIC, &t_start);
 
     /* Relaxation: repeat until no cell changes */
     bool changed = true;
     while (changed) {
         changed = false;
         for (int y = 1; y <= height; y++) {
             for (int x = 1; x <= width; x++) {
                 /* Compute next state and accumulate change flag */
                 changed |= sync_compute_new_state(sand, next, cols, y, x);
             }
         }
         /* Swap buffers: 'next' becomes current, old 'sand' reused */
         int *tmp = sand;
         sand = next;
         next = tmp;
     }
 
     clock_gettime(CLOCK_MONOTONIC, &t_end);
     double elapsed = (t_end.tv_sec - t_start.tv_sec)
                     + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
     fprintf(stderr, "Relaxation runtime: %.6f seconds\n", elapsed);
 
     /* Write the final stable sandpile to a binary PPM (P6) */
     FILE *fp = fopen("sandpile.ppm", "wb");
     if (!fp) {
         perror("fopen");
         return EXIT_FAILURE;
     }
     /* P6 header: width height, max colour 255 */
     fprintf(fp, "P6\n%d %d\n255\n", width, height);
 
     /* Emit pixel at each interior cell based on its final value */
     for (int y = 1; y <= height; y++) {
         for (int x = 1; x <= width; x++) {
             int v = sand[y * cols + x];
             unsigned char r, g, b;
             switch (v) {
                 case 0: r = 0;   g = 0;   b = 0;   break;  /* black */
                 case 1: r = 0;   g = 255; b = 0;   break;  /* green */
                 case 2: r = 0;   g = 0;   b = 255; break;  /* blue  */
                 case 3: r = 255; g = 0;   b = 0;   break;  /* red   */
                 default:
                     r = g = b = 0;  /* should not occur */
             }
             fputc(r, fp);
             fputc(g, fp);
             fputc(b, fp);
         }
     }
     fclose(fp);
     fprintf(stderr, "Wrote sandpile.ppm (%dx%d)\n", width, height);
 
     /* Free allocated memory */
     free(sand);
     free(next);
     return EXIT_SUCCESS;
 }