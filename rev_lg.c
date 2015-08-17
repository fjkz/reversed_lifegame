#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DEAD  0
#define ALIVE 1
#define EMPTY 2

#define NUM_CANDIDATES_9CELLS_ALIVE 140 // 9C3 + 8C3
#define NUM_CANDIDATES_9CELLS_DEAD  372 // 2^9 - 136

// List of the previous 3x3 cells paterns for each center cell state.
static char **prev9cells_alive;
static char **prev9cells_dead;

int pop(int x)
{
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return x & 0x0000003F;
}

int comp_pop(const void *a, const void *b)
{
    int i = *(int *)a;
    int j = *(int *)b;

    return pop(i) - pop(j); 
}

// Initialize 3x3 cell paterns.
void initialize()
{
    // Memory allocation for 2D array.
    prev9cells_alive = (char **)malloc(
            sizeof(char *) * NUM_CANDIDATES_9CELLS_ALIVE);

    for (int i = 0; i < NUM_CANDIDATES_9CELLS_ALIVE; i++) {
        prev9cells_alive[i] = (char *)malloc(sizeof(char) * 9);
    }

    prev9cells_dead = (char **)malloc(
            sizeof(char *) * NUM_CANDIDATES_9CELLS_DEAD);

    for (int i = 0; i < NUM_CANDIDATES_9CELLS_DEAD; i++) {
        prev9cells_dead[i] = (char *)malloc(sizeof(char) * 9);
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
                prev9cells_alive[na][j] = c[j];
            }
            na++;

        // The next cell is dead.
        } else {
            for (int j = 0; j < 9; j++) {
                prev9cells_dead[nd][j] = c[j];
            }
            nd++;
        }
    }

    assert(na == NUM_CANDIDATES_9CELLS_ALIVE);
    assert(nd == NUM_CANDIDATES_9CELLS_DEAD);
}

// Print field with PBM format.
void print_field(const char *field, int nx, int ny)
{
    if (field == NULL) {
        fprintf(stderr, "Argument field is NULL.\n");
        return;
    }

    printf("P1\n");
    printf("%d %d\n", nx, ny);

    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            printf("%d ", field[i + nx * j]);
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
int match9cells(const char *field, int nx, int ny,
                const char *cells, int pos)
{
    int x = pos % nx;
    int y = pos / nx;

    for (int i = 0; i < 9; i++) {
        int p = get_pos(x + cx[i], y + cy[i], nx, ny);
        char f = field[p];
        if (f != EMPTY && f != cells[i]) {
            return UNMACHED;
        }
    }

    return MACHED;
}

// Overwrite the field with the given 3x3 cells pattern.
static inline
char *overwrite9cells(char *field, int nx, int ny,
                      const char *cells, int pos)
{
    int x = pos % nx;
    int y = pos / nx;

    for (int i = 0; i < 9; i++) {
        int p = get_pos(x + cx[i], y + cy[i], nx, ny);
        field[p] = cells[i];
    }

    return field;
}

static int count_called = 0;

// Find previos cells recursively.
static
char *_prev_field(const char *field, int nx, int ny,
                  char *p_field, int pos,
                  int *progress)
{
    count_called++;

    // Select candidates with the cell state.
    char **prev9cells;
    int num_cand;

    if (field[pos] == ALIVE) {
        prev9cells = prev9cells_alive;
        num_cand = NUM_CANDIDATES_9CELLS_ALIVE;
    } else {
        prev9cells = prev9cells_dead;
        num_cand = NUM_CANDIDATES_9CELLS_DEAD;
    }

    const int size_p_field = sizeof(char) * nx * ny;
    const int pos_end = nx * ny - 1;

    // For all 3x3 cells pattern
    // whose center cell becomes the given field state.
    //
    // Search from leaner patterns
    // because they appear more frequently.
    //
    // Restart from the remembered candidate.
    for (int i = progress[pos]; i < num_cand; i++) {
        // Check the canditate macthes with the existed cells.
        if (match9cells(p_field, nx, ny, prev9cells[i], pos) == UNMACHED) {
            continue;
        }

        // Copy of the previous field candidate.
        char *p_field_ = (char *)malloc(size_p_field);
        memcpy(p_field_, p_field, size_p_field);

        p_field_ = overwrite9cells(p_field_, nx, ny, prev9cells[i], pos); 

        // The search is succeed if all the cells are covered.
        // Search from the next candidate at the next call.
        if (pos == pos_end) {
            progress[pos] = i + 1;
            free(p_field);
            return p_field_;
        }

        // Find the previous pattern for the next cell.
        p_field_ = _prev_field(field, nx, ny, p_field_, pos + 1, progress);

        // No pattern found. Try with the next candidate.
        if (p_field_ == NULL) {
            continue;
        }

        // A suitable pattern was found.
        // Search from the this candidate at the next call.
        progress[pos] = i;
        free(p_field);
        return p_field_;
    }

    // All candidate is not suitable.
    // Search from the first candidate at the next call
    // because the candidate of the previous position is changed.
    progress[pos] = 0;
    free(p_field);
    return NULL;
}

// Get a field pattern that becomes to
// the given field pattern at the next step.
char *prev_field(const char *field, int nx, int ny, int *progress)
{
    char *p_field = (char *)malloc(sizeof(char) * nx * ny);

    for (int i = 0; i < nx * ny; i++) {
        p_field[i] = EMPTY;
    }

    p_field = _prev_field(field, nx, ny, p_field, 0, progress);

    return p_field;
}

// TODO: multi thead.
char *ansistor_field(const char *field, int nx, int ny, int num_back)
{
    fprintf(stderr, "#");

    // Count the progress of searching.
    int progress[nx * ny];
    for (int i = 0; i < nx * ny; i++) {
        progress[i] = 0;
    }

    while (1) {
        char *p_field = prev_field(field, nx, ny, progress);

        // No suitable pattern.
        if (p_field == NULL) {
            fprintf(stderr, "\b");
            return NULL;
        }

        if (num_back == 1) {
            fprintf(stderr, "\n\n");
            return p_field;
        }

        char *a_field = ansistor_field(p_field, nx, ny, num_back - 1);
        free(p_field);

        // Try the next previous field.
        if (a_field == NULL) {
            continue;
        }

        return a_field;
    }
}

int main(int argc, char *argv[])
{
    initialize();

    char field[] = {
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      };

    char *p_field = ansistor_field(field, 5, 5, 10);

    if (p_field == NULL) {
        fprintf(stderr, "No previous field pattern was found.\n");
        return 1;
    }

    print_field(p_field, 5, 5);

    fprintf(stderr, "<_prev_field> is called %d times.\n", count_called);

    return 0;
}
