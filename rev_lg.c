#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// The next state of the center cell for each 3x3 cell.
static char next_cell[512];

static int pop(int x)
{
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return x & 0x0000003F;
}

// Initialize 3x3 cell paterns.
void initialize()
{
    // Check next center cell state for each 3x3 cells.
    for (int i = 0; i < 512; i++) {
        int sum = pop(i);

        if (sum == 3 ||
            (sum == 4 && i & 0x10)) { // bit of the center cell.
            next_cell[i] = 1;
        } else {
            next_cell[i] = 0;
        }
    }
}

// Cells are sorted as (x, y) =
// (0, 0), (1, 0) ... (nx-1, 0) (0, 1) ... (nx-2, ny-1), (nx-1, ny-1)
// cell[i] = 1 means alive, 0 meas dead.
struct field {
    char *cell; // 1 means alive, 0 means dead.
    int nx;
    int ny;
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
int _prev_cell(struct field f, char *p_cell, int pos, int *progress)
{
    count_prev_cell++;

    const char target = f.cell[pos];

    const int nx = f.nx;
    const int ny = f.ny;

    // Check which edge "pos" is on.
    //
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
    //
    //   --> x
    // | 0 1 2
    // v 3 4 5
    // y 6 7 8
    //
    int c0 = edge & (EDGE_S | EDGE_E) ? 0 : p_cell[pos - 1 - nx];
    int c1 = edge & EDGE_S            ? 0 : p_cell[pos     - nx];
    int c2 = edge & (EDGE_S | EDGE_W) ? 0 : p_cell[pos + 1 - nx];
    int c3 = edge & EDGE_E            ? 0 : p_cell[pos - 1     ];
    int c4 =                                p_cell[pos         ];
    int c5 = edge & EDGE_W            ? 0 : p_cell[pos + 1     ];
    int c6 = edge & (EDGE_N | EDGE_E) ? 0 : p_cell[pos - 1 + nx];
    int c7 = edge & EDGE_N            ? 0 : p_cell[pos     + nx];
    int c8 = edge & (EDGE_N | EDGE_W) ? 0 : p_cell[pos + 1 + nx];

    int alive = c0 << 0 | c1 << 1 | c2 << 2 |
                c3 << 3 | c4 << 4 | c5 << 5 |
                c6 << 6 | c7 << 7 | c8 << 8;

    int cand[16];
    int num_cand;

    switch (edge) {

    // On EDGE_N or EDGE_W cells are filled.
    // Check the existing 3x3 cells are suitable.
    default: // EDGE_N or EDGE_W
        if (next_cell[alive] == target) {
            if (progress[pos] > 0) {
                progress[pos] = 0;
                return 1;
            }
            return _prev_cell(f, p_cell, pos + 1, progress);
        } else {
            progress[pos] = 0;
            return 1;
        }

    case EDGE_N | EDGE_W:
        if (next_cell[alive] == target) {
            // The search is succeed if all the cells are covered.
            // The next search restarts from the next candidate.
            progress[nx * (ny - 1) - 1]++;
            return 0;
        } else {
            return 1;
        }

    // Cells written in the previous turns.
    int non_empty;

    // Extract for-loop because this case is the most frequent
    // and the number of candidate is few.
    case EDGE_O:
        non_empty = 1 << 0 | 1 << 1 | 1 << 2 | // 1 1 1
                    1 << 3 | 1 << 4 | 1 << 5 | // 1 1 1
                    1 << 6 | 1 << 7 | 0 << 8;  // 1 1 0
        alive &= non_empty;

        switch (progress[pos]) {
        int cell9;

        // The empty cell is dead
        case 0:
            cell9 = alive | (0 << 8);
            if (next_cell[cell9] == target) {
                p_cell[pos + 1 + nx] = 0;
                int ret = _prev_cell(f, p_cell, pos + 1, progress);
                if (!ret) {
                    // Success. Restart from this case.
                    return 0;
                }
            }
            // Fail. Try case 1.

        // The empty cell is alive.
        case 1:
            cell9 = alive | (1 << 8);
            if (next_cell[cell9] == target) {
                p_cell[pos + 1 + nx] = 1;
                int ret = _prev_cell(f, p_cell, pos + 1, progress);
                if (!ret) {
                    // Success. Restart from this case.
                    progress[pos] = 1;
                    return 0;
                }
            }

            // All cases are not suitable.
            progress[pos] = 0;
            return 1;
        }

    // In the cases of EDGE_S or EDGE_E, set "num_and" and "cand[i]".
    // Search about the candidates in the next for-loop.
    case EDGE_S | EDGE_E:
        non_empty = 1 << 0 | 1 << 1 | 1 << 2 | // 1 1 1
                    1 << 3 | 0 << 4 | 0 << 5 | // 1 0 0
                    1 << 6 | 0 << 7 | 0 << 8;  // 1 0 0
        alive &= non_empty;
        num_cand = 16;
        for (int i = 0; i < 16; i++)
            cand[i] = (i & 0x3) << 4 | (i & 0xC) << 5;
        break;

    case EDGE_S:
        non_empty = 1 << 0 | 1 << 1 | 1 << 2 | // 1 1 1
                    1 << 3 | 1 << 4 | 0 << 5 | // 1 1 0
                    1 << 6 | 1 << 7 | 0 << 8;  // 1 1 0
        alive &= non_empty;
        num_cand = 4;
        for (int i = 0; i < 4; i++)
            cand[i] = (i & 0x1) << 5 | (i & 0x2) << 7;
        break;

    case EDGE_E:
        non_empty = 1 << 0 | 1 << 1 | 1 << 2 | // 1 1 1
                    1 << 3 | 1 << 4 | 1 << 5 | // 1 1 1
                    1 << 6 | 0 << 7 | 0 << 8;  // 1 0 0
        alive &= non_empty;
        num_cand = 4;
        for (int i = 0; i < 4; i++)
            cand[i] = i << 7; 
        break; 
    }

    // For all 3x3 cells pattern that is compatible
    // with the cells written in previous turns.
    //
    // Restart from the remembered candidate.
    for (int i = progress[pos]; i < num_cand; i++) {
        const int cell9 = alive | cand[i];

        // Check the canditate is suitable.
        if (next_cell[cell9] != target) {
            // Not good. Try the next candidate.
            continue;
        }

        switch (edge) {
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
        }

        // Find the previous pattern for the next cell.
        int ret = _prev_cell(f, p_cell, pos + 1, progress);

        // No pattern found. Try with the next candidate.
        // Does not need to revert p_cell because check only non-empty cells.
        if (ret) {
            continue;
        }

        // A suitable pattern was found.
        // Search from the this candidate at the next call.
        count_prev_cell_for += i - progress[pos] + 1;
        progress[pos] = i;
        return 0;
    }

    // All candidate is not suitable.
    // Search from the first candidate at the next call
    // because the candidate of the previous position is changed.
    count_prev_cell_for += num_cand - progress[pos];
    progress[pos] = 0;
    return 1;
}

// Get a field pattern that becomes to
// the given field pattern at the next step.
char *prev_cell(struct field f, int *progress)
{
    // Faster than malloc ?
    char *p_cell = (char *) calloc(sizeof(char), f.nx * f.ny);

    int ret = _prev_cell(f, p_cell, /* pos = */ 0 , progress);

    if (ret) {
        free(p_cell);
        return NULL;
    } else {
        return p_cell;
    }
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
            return NULL;
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

    char _ = 0;
    char X = 1;

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

    // Print statistics.
    fprintf(stderr, "<ansistor_field> is called %lu times. \n",
            count_ansistor_field);
    fprintf(stderr, "<_prev_cell> is called %lu times.\n",
            count_prev_cell);
    fprintf(stderr, "<_prev_cell> for-loop is called %lu times.\n",
            count_prev_cell_for);

    if (f.cell == NULL) {
        fprintf(stderr, "No previous field pattern was found.\n");
        return 1;
    }

    print_field(f);

    return 0;
}
