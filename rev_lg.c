#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DEAD     0
#define ALIVE    1
#define EMPTY    2
#define NORESULT 3

#define NUM_CANDIDATES_CELL9_ALIVE 140 // 9C3 + 8C3
#define NUM_CANDIDATES_CELL9_DEAD  372 // 2^9 - 136

// List of the previous 3x3 cells paterns for each center cell state.
static char **prev_cell9_alive;
static char **prev_cell9_dead;

static int pop(int x)
{
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return x & 0x0000003F;
}

static int comp_pop(const void *a, const void *b)
{
    int i = *(int *)a;
    int j = *(int *)b;

    return pop(i) - pop(j); 
}

// Initialize 3x3 cell paterns.
void initialize()
{
    // Memory allocation for 2D array.
    prev_cell9_alive = (char **)malloc(
            sizeof(char *) * NUM_CANDIDATES_CELL9_ALIVE);

    for (int i = 0; i < NUM_CANDIDATES_CELL9_ALIVE; i++) {
        prev_cell9_alive[i] = (char *)malloc(sizeof(char) * 9);
    }

    prev_cell9_dead = (char **)malloc(
            sizeof(char *) * NUM_CANDIDATES_CELL9_DEAD);

    for (int i = 0; i < NUM_CANDIDATES_CELL9_DEAD; i++) {
        prev_cell9_dead[i] = (char *)malloc(sizeof(char) * 9);
    } 

    // Counters for each state of the center cell.
    int na = 0;
    int nd = 0;

    int integers[512]; /* 512 = 2^9 */
    for (int i = 0; i < 512; i++) {
        integers[i] = i;
    }

    // Sort with population counts.
    qsort(integers, 512, sizeof(int), comp_pop);

    // Check next center cell state for each 3x3 cells.
    for (int i = 0; i < 512; i++) {
        char c[9];
        char sum = 0;

        for (int j = 0; j < 9; j++) {
            // Get the j-th bit in i.
            c[j] = (integers[i] >> j) & 1;
            sum += c[j];
        }

        // The next cell is alive.
        if (sum == 3 || (sum == 4 && c[4] == ALIVE)) {
            for (int j = 0; j < 9; j++) {
                prev_cell9_alive[na][j] = c[j];
            }
            na++;

        // The next cell is dead.
        } else {
            for (int j = 0; j < 9; j++) {
                prev_cell9_dead[nd][j] = c[j];
            }
            nd++;
        }
    }

    assert(na == NUM_CANDIDATES_CELL9_ALIVE);
    assert(nd == NUM_CANDIDATES_CELL9_DEAD);
}

struct field {
    char *cell;
    int nx;
    int ny;
};

// Print field with PBM format.
void print_field(struct field f)
{
    printf("P1\n");
    printf("%d %d\n", f.nx, f.ny);

    for (int j = 0; j < f.ny; j++) {
        for (int i = 0; i < f.nx; i++) {
            printf("%d ", f.cell[i + f.nx * j]);
        }
        printf("\n");
    }
}

// Get 1d position. Assumes the field has periodic boundaries.
static inline
int get_pos(int x, int y, int nx, int ny)
{
    if (x >= nx) {
        x = x % nx;
    } else if (x < 0) {
        x = x + nx;
    }

    if (y >= ny) {
        y = y % ny;
    } else if (y < 0) {
        y = y + ny;
    }

    return x + nx * y;
}

#define MACHED   0
#define UNMACHED 1

//   -> x
// | 0 1 2
// v 3 4 5
// y 6 7 8
static const int cx[9] = {-1,  0,  1, -1,  0,  1, -1,  0,  1};
static const int cy[9] = {-1, -1, -1,  0,  0,  0,  1,  1,  1};

// Check the field is mathed to given 3x3 cells pattern.
static inline
int match_cell9(const char *cell, int nx, int ny,
                const char *cell9, int pos)
{
    int x = pos % nx;
    int y = pos / nx;

    for (int i = 0; i < 9; i++) {
        int p = get_pos(x + cx[i], y + cy[i], nx, ny);
        char f = cell[p];
        if (f != EMPTY && f != cell9[i]) {
            return UNMACHED;
        }
    }

    return MACHED;
}

// Overwrite the field with the given 3x3 cells pattern.
static inline
void overwrite_cell9(char *cell, int nx, int ny,
                     const char *cell9, int pos)
{
    int x = pos % nx;
    int y = pos / nx;

    for (int i = 0; i < 9; i++) {
        int p = get_pos(x + cx[i], y + cy[i], nx, ny);
        cell[p] = cell9[i];
    }
}

static unsigned long count_called = 0;

// Find previos cells recursively.
static
void _prev_cell(struct field f, char *p_cell, int pos, int *progress)
{
    count_called++;

    // Select candidates with the cell state.
    char **prev_cell9;
    int num_cand;

    if (f.cell[pos] == ALIVE) {
        prev_cell9 = prev_cell9_alive;
        num_cand = NUM_CANDIDATES_CELL9_ALIVE;
    } else {
        prev_cell9 = prev_cell9_dead;
        num_cand = NUM_CANDIDATES_CELL9_DEAD;
    }

    const int nx = f.nx;
    const int ny = f.ny;
    const int size_cell = sizeof(char) * nx * ny;
    const int pos_end = nx * ny - 1;

    // Copy of the previous field candidate.
    char p_cell_cpy[nx*ny];
    memcpy(p_cell_cpy, p_cell, size_cell);

    // For all 3x3 cells pattern
    // whose center cell becomes the given field state.
    //
    // Search from leaner patterns
    // because they appear more frequently.
    //
    // Restart from the remembered candidate.
    for (int i = progress[pos]; i < num_cand; i++) {
        // Check the canditate macthes with the existed cells.
        if (match_cell9(p_cell, nx, ny, prev_cell9[i], pos) == UNMACHED) {
            continue;
        }

        overwrite_cell9(p_cell, nx, ny, prev_cell9[i], pos); 

        // The search is succeed if all the cells are covered.
        // Search from the next candidate at the next call.
        if (pos == pos_end) {
            progress[pos] = i + 1;
            return;
        }

        // Find the previous pattern for the next cell.
        _prev_cell(f, p_cell, pos + 1, progress);

        // No pattern found. Try with the next candidate.
        if (p_cell[0] == NORESULT) {
            // Revert p_cell
            memcpy(p_cell, p_cell_cpy, size_cell);
            continue;
        }

        // A suitable pattern was found.
        // Search from the this candidate at the next call.
        progress[pos] = i;
        return;
    }

    // All candidate is not suitable.
    // Search from the first candidate at the next call
    // because the candidate of the previous position is changed.
    progress[pos] = 0;
    p_cell[0] = NORESULT;
    return;
}

// Get a field pattern that becomes to
// the given field pattern at the next step.
char *prev_cell(struct field f, int *progress)
{
    int l = f.nx * f.ny;
    char *p_cell = (char *)malloc(sizeof(char) * l);

    for (int i = 0; i < l; i++) {
        p_cell[i] = EMPTY;
    }

    _prev_cell(f, p_cell, /* pos = */ 0 , progress);

    return p_cell[0] == NORESULT ? NULL : p_cell;
}

// TODO: multi thead.
char *ansistor_field(struct field f, int back)
{
    fprintf(stderr, "#");

    // Count the progress of searching.
    int l = f.nx * f.ny;
    int progress[l];
    for (int i = 0; i < l; i++) {
        progress[i] = 0;
    }

    struct field pf = {NULL, f.nx, f.ny};

    while (1) {
        char *p_cell = prev_cell(f, progress);

        // No suitable pattern.
        if (p_cell == NULL) {
            fprintf(stderr, "\b");
            return p_cell;
        }

        if (back == 1) {
            fprintf(stderr, "\n\n");
            return p_cell;
        }

        pf.cell = p_cell;

        char *a_cell = ansistor_field(pf, back - 1);

        free(p_cell);

        // Try the next previous field.
        if (a_cell == NULL) {
            continue;
        }

        return a_cell;
    }
}

int main(int argc, char *argv[])
{
    initialize();

    char cell[] = {
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0
    };

    struct field f = {
      .cell = cell,
      .nx = 5,
      .ny = 5
    };

    int back = 10;

    f.cell = ansistor_field(f, back);

    if (f.cell == NULL) {
        fprintf(stderr, "No previous field pattern was found.\n");
        return 1;
    }

    print_field(f);

    fprintf(stderr, "<_prev_field> is called %lu times.\n", count_called);

    return 0;
}
