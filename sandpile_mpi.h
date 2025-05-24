#ifndef SANDPILE_H
#define SANDPILE_H

#include <stdbool.h>

/* Interior grid dimensions */
#ifndef N
#define N 1000   /* Number of rows */
#endif

#ifndef M
#define M 1000   /* Number of columns */
#endif

/* Total grid size including a one‐cell “ghost” border around */
#define ROWS (N + 2)
#define COLS (M + 2)

/**
 * Convert a 2D (row, col) into a 1D array index.
 */
#define IDX(r, c) ((r) * COLS + (c))

#endif /* SANDPILE_H */
