#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NUM_CANDIDATES_CELL9_ALIVE 140 // 9C3 + 8C3
#define NUM_CANDIDATES_CELL9_DEAD  372 // 2^9 - 136

// List of the previous 3x3 cells paterns for each center cell state.
static int *prev_cell9_alive;
static int *prev_cell9_dead;

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
    prev_cell9_alive = (int *) malloc(
            sizeof(int) * NUM_CANDIDATES_CELL9_ALIVE);

    prev_cell9_dead = (int *) malloc(
            sizeof(int) * NUM_CANDIDATES_CELL9_DEAD);

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
        int x = integers[i];
        int sum = pop(integers[i]);

        // The next cell is alive.
        if (sum == 3 || (sum == 4 &&
                         (x >> 4) & 1)) { // bit of the center cell.
            prev_cell9_alive[na] = x;
            na++;

        // The next cell is dead.
        } else {
            prev_cell9_dead[nd] = x;
            nd++;
        }
    }

    assert(na == NUM_CANDIDATES_CELL9_ALIVE);
    assert(nd == NUM_CANDIDATES_CELL9_DEAD);
}

#define DEAD     0x0
#define ALIVE    0x1
#define EMPTY    0x2
#define NORESULT 0x4

struct field {
    char *cell; // Includes edge. Edge cells are dead.
    int nx;     // Not includes edge.
    int ny;     // Not includes edge.
};

// Print field with PBM format.
void print_field(struct field f)
{
    const int nx = f.nx;
    const int ny = f.ny;

    printf("P1\n");
    printf("%d %d\n", nx, ny);

    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            printf("%d ", f.cell[i + nx * j]);
        }
        printf("\n");
    }
}

#define EDGE_O 0x0
#define EDGE_E 0x1
#define EDGE_W 0x2
#define EDGE_S 0x4
#define EDGE_N 0x8

static unsigned long count_prev_cell = 0;
static unsigned long count_prev_cell_for = 0;

// Find previos cells recursively.
static
void _prev_cell(struct field f, char *p_cell, int pos, int *progress)
{
    count_prev_cell++;

    // Select candidates with the cell state.
    int *prev_cell9;
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

    //    --> x
    //   |    __S__
    //   v   |     |
    //  y   E|  O  |W
    //       |_____|
    //          N
    int edge = EDGE_O;

    const int x = pos % nx;
    if (x == 0)
        edge |= EDGE_E;
    else if (x == nx - 1)
        edge |= EDGE_W;

    const int y = pos / nx;
    if (y == 0)
        edge |= EDGE_S;
    else if (y == ny - 1)
        edge |= EDGE_N;

    // Get the 3x3 cells to be search at this turn.
    // The boundaries are dead.
    int c0 = edge & (EDGE_S | EDGE_E) ? DEAD : p_cell[pos - 1 - nx];
    int c1 = edge & EDGE_S            ? DEAD : p_cell[pos     - nx];
    int c2 = edge & (EDGE_S | EDGE_W) ? DEAD : p_cell[pos + 1 - nx];
    int c3 = edge & EDGE_E            ? DEAD : p_cell[pos - 1     ];
    int c4 =                                   p_cell[pos         ];
    int c5 = edge & EDGE_W            ? DEAD : p_cell[pos + 1     ];
    int c6 = edge & (EDGE_N | EDGE_E) ? DEAD : p_cell[pos - 1 + nx];
    int c7 = edge & EDGE_N            ? DEAD : p_cell[pos     + nx];
    int c8 = edge & (EDGE_N | EDGE_W) ? DEAD : p_cell[pos + 1 + nx];

    // The 0 1, 2, 3, 6 th is not empty clearly.
    const int empty = (c4 & 2) << 3 |
                      (c5 & 2) << 4 |
                      (c7 & 2) << 6 |
                      (c8 & 2) << 7;

    const int non_empty = ~empty;

    const int alive = (c0 & 1)      |
                      (c1 & 1) << 1 |
                      (c2 & 1) << 2 |
                      (c3 & 1) << 3 |
                      (c4 & 1) << 4 |
                      (c5 & 1) << 5 |
                      (c6 & 1) << 6 |
                      (c7 & 1) << 7 |
                      (c8 & 1) << 8;

    // Copy of the previous field candidate.
    // If domain size is larger, stack may overflow.
    // If so, revert only overwritten cells manulally.
    char p_cell_cpy[nx * ny];
    const int size_cell = sizeof(char) * nx * ny;
    memcpy(p_cell_cpy, p_cell, size_cell);

    // For all 3x3 cells pattern // TODO: hack not select, build 
    // whose center cell becomes the given field state.
    //
    // Search from leaner patterns
    // because they appear more frequently.
    //
    // Restart from the remembered candidate.
    for (int i = progress[pos]; i < num_cand; i++) {
        const int cell9 = prev_cell9[i];

        // Check the canditate macthes with the existed cells.
        if ((alive ^ cell9) & non_empty) {
            // Not matched. Try the next candidate.
            continue;
        }

        switch (edge) {
        // The search is succeed if all the cells are covered.
        // Search from the next candidate at the next call.
        case EDGE_N | EDGE_W:
            count_prev_cell_for += i - progress[pos] + 1;
            progress[pos] = i + 1;
            return;

        // Overwrites the field with the given 3x3 cells pattern.
        // Write to only empty cells.
        case EDGE_S | EDGE_E:
            p_cell[pos         ] = (cell9 >> 4) & 1;
            p_cell[pos + 1     ] = (cell9 >> 5) & 1;
            p_cell[pos     + nx] = (cell9 >> 7) & 1;
            p_cell[pos + 1 + nx] = (cell9 >> 8) & 1;
            break;

        case EDGE_S:
            p_cell[pos + 1     ] = (cell9 >> 5) & 1;
            p_cell[pos + 1 + nx] = (cell9 >> 8) & 1;
            break;

        case EDGE_E:
            p_cell[pos     + nx] = (cell9 >> 7) & 1;
            p_cell[pos + 1 + nx] = (cell9 >> 8) & 1;
            break;

        case EDGE_O:
            p_cell[pos + 1 + nx] = (cell9 >> 8) & 1;
            break;
        }

        // Find the previous pattern for the next cell.
        _prev_cell(f, p_cell, pos + 1, progress);

        // No pattern found. Try with the next candidate.
        if (p_cell[0] & NORESULT) {
            // Revert p_cell
            memcpy(p_cell, p_cell_cpy, size_cell);
            continue;
        }

        // A suitable pattern was found.
        // Search from the this candidate at the next call.
        count_prev_cell_for += i - progress[pos] + 1;
        progress[pos] = i;
        return;
    }

    // All candidate is not suitable.
    // Search from the first candidate at the next call
    // because the candidate of the previous position is changed.
    count_prev_cell_for += num_cand - progress[pos];
    progress[pos] = 0;
    p_cell[0] |= NORESULT;
    return;
}

// Get a field pattern that becomes to
// the given field pattern at the next step.
char *prev_cell(struct field f, int *progress)
{
    int l = f.nx * f.ny;
    char *p_cell = (char *)calloc(sizeof(char), l);

    for (int i = 0; i < l; i++) {
        p_cell[i] = EMPTY;
    }

    _prev_cell(f, p_cell, /* pos = */ 0 , progress);

    return p_cell[0] & NORESULT ? NULL : p_cell;
}

unsigned long count_ansistor_field = 0;

// TODO: multi thead.
char *ansistor_field(struct field f, int back)
{
    count_ansistor_field++;

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
            fprintf(stderr, "\n");
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

    char _ = DEAD;
    char X = ALIVE;

    char cell[] = {
      _,_,_,_,_,
      _,X,X,_,_,
      _,X,_,X,_,
      _,_,X,X,_,
      _,_,_,_,_,
    };

    struct field f = {
      .cell = cell,
      .nx = 5,
      .ny = 5
    };

    int back = 15;

    f.cell = ansistor_field(f, back);

    if (f.cell == NULL) {
        fprintf(stderr, "No previous field pattern was found.\n");
        return 1;
    }

    print_field(f);

    fprintf(stderr, "<ansistor_field> is called %lu times. \n",
            count_ansistor_field);
    fprintf(stderr, "<_prev_cell> is called %lu times.\n",
            count_prev_cell);
    fprintf(stderr, "<_prev_cell> for-loop is called %lu times.\n",
            count_prev_cell_for);

    return 0;
}
