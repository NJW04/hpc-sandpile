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

 #include <stdio.h>
 #include <stdlib.h>
 #include <stdbool.h>
 
 #ifndef N
 #define N 512   /* number of interior rows */
 #endif
 
 #ifndef M
 #define M 512   /* number of interior columns */
 #endif
 
 static inline int sync_compute_new_state(int *sand, int *next, int cols, int y, int x) {
     int idx = y * cols + x;
     next[idx] = sand[idx] % 4
               + sand[idx - 1]   / 4
               + sand[idx + 1]   / 4
               + sand[idx - cols]/ 4
               + sand[idx + cols]/ 4;
     return next[idx] != sand[idx];
 }
 
 int main(int argc, char *argv[]) {
     const int height = N;
     const int width  = M;
     const int rows = height + 2;  /* +2 for sink border */
     const int cols = width  + 2;
 
     /* Allocate grids */
     int *sand = malloc(rows * cols * sizeof(int));
     int *next = malloc(rows * cols * sizeof(int));
     if (!sand || !next) {
         perror("malloc");
         return EXIT_FAILURE;
     }
 
     /* Zero‐initialize */
     for (int i = 0; i < rows * cols; i++) {
         sand[i] = next[i] = 0;
     }
 
     /* Initial pile: all cells = 4 (unstable) or single‐center variant?
        Here we do the “all‐4” case; adapt if needed. */
     for (int y = 1; y <= height; y++)
         for (int x = 1; x <= width; x++)
             sand[y * cols + x] = 4;
 
     /* Alternatively, to match Figure 1: one centre cell = width*height, rest = 0:
     int cy = height/2 + 1, cx = width/2 + 1;
     sand[cy*cols + cx] = width * height;
     */
 
     /* Relaxation loop */
     bool changed = true;
     while (changed) {
         changed = false;
         for (int y = 1; y <= height; y++) {
             for (int x = 1; x <= width; x++) {
                 changed |= sync_compute_new_state(sand, next, cols, y, x);
             }
         }
         /* swap buffers */
         int *tmp = sand; sand = next; next = tmp;
     }
 
     /* Write P6 PPM */
     FILE *fp = fopen("sandpile.ppm", "wb");
     if (!fp) {
         perror("fopen");
         return EXIT_FAILURE;
     }
     fprintf(fp, "P6\n%d %d\n255\n", width, height);
 
     for (int y = 1; y <= height; y++) {
         for (int x = 1; x <= width; x++) {
             unsigned char r, g, b;
             int v = sand[y * cols + x];
             switch (v) {
                 case 0: r = 0;   g = 0;   b = 0;   break;  // black
                 case 1: r = 0;   g = 255; b = 0;   break;  // green
                 case 2: r = 0;   g = 0;   b = 255; break;  // blue
                 case 3: r = 255; g = 0;   b = 0;   break;  // red
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
 