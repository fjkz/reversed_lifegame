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

// Initialize 3x3 cell paterns.
void initialize()
{
    // Memory allocation for 2D array.
    prev9cells_alive = (char **)malloc(
            sizeof(char *) * NUM_CANDIDATES_9CELLS_ALIVE);
    for (int i = 0; i < NUM_CANDIDATES_9CELLS_ALIVE; i++)
        prev9cells_alive[i] = (char *)malloc(sizeof(char) * 9);

    prev9cells_dead = (char **)malloc(
            sizeof(char *) * NUM_CANDIDATES_9CELLS_DEAD);
    for (int i = 0; i < NUM_CANDIDATES_9CELLS_DEAD; i++)
        prev9cells_dead[i] = (char *)malloc(sizeof(char) * 9);
    
    int na = 0;
    int nd = 0;

    // Check next center cell state for each 3x3 cells.
    for (int i = 0; i < 512 /* = 2^9 */; i++) {
        char c[9];
        char sum = 0;
        for (int j = 0; j < 9; j++) {
            // Get the j-th bit in i.
            c[j] = (i >> (8 - j)) & 1;
            sum += c[j];
        }

        // The next cell is alive.
        if (sum == 3 || (sum == 4 && c[4] == ALIVE)) {
            for (int j = 0; j < 9; j++)
                prev9cells_alive[na][j] = c[j];
            na++;

        // The next cell is dead.
        } else {
            for (int j = 0; j < 9; j++)
                prev9cells_dead[nd][j] = c[j];
            nd++;
        }
    }

    assert(na == NUM_CANDIDATES_9CELLS_ALIVE);
    assert(nd == NUM_CANDIDATES_9CELLS_DEAD);
}

void print_field(char *field, int nx, int ny)
{
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            char c;
            switch (field[i + nx * j]) {
            case ALIVE:
                c = '*';
                break;
            case DEAD:
                c = '.';
                break;
            default:
                c = '~';
                break;
            }
            printf("%c ", c);
        }
        printf("\n");
    }
}

// Get corrected position. Assumes the field has periodic boundaries.
int correct_xy(int x, int nx)
{
    if (x >= nx) {
        x = x % nx;
    } else if (x < 0) {
        x = x + nx;
    }

    return x;
}

#define MACHED   0
#define UNMACHED 1

// Check the field is mathed to given 3x3 cells pattern.
int match9cells(const char *field, int nx, int ny,
                const char *cells, int pos)
{
    int x = pos % nx;
    int y = pos / nx;

    //   -> x
    // | 0 1 2
    // v 3 4 5
    // y 6 7 8
    int cx[9] = {-1,  0,  1, -1,  0,  1, -1,  0,  1};
    int cy[9] = {-1, -1, -1,  0,  0,  0,  1,  1,  1};

    for (int i = 0; i < 9; i++) {
        int x_ = correct_xy(x + cx[i], nx);
        int y_ = correct_xy(y + cy[i], ny);
        int p_ = x_ + nx * y_;

        if (field[p_] != EMPTY && field[p_] != cells[i])
            return UNMACHED;
    }

    return MACHED;
}

// Overwrite the field with the given 3x3 cells pattern.
char *overwrite9cells(char *field, int nx, int ny,
                      const char *cells, int pos)
{
    int x = pos % nx;
    int y = pos / nx;

    int cx[9] = {-1,  0,  1, -1,  0,  1, -1,  0,  1};
    int cy[9] = {-1, -1, -1,  0,  0,  0,  1,  1,  1};

    for (int i = 0; i < 9; i++) {
        int x_ = correct_xy(x + cx[i], nx);
        int y_ = correct_xy(y + cy[i], ny);
        int p_ = x_ + nx * y_;

        field[p_] = cells[i];
    }

    return field;
}

// Find previos cells recursively.
static char *_prev_field(const char *field, int nx, int ny,
                         char *p_field, int pos)
{
    // The search is succeed if all the cells are covered.
    if (pos >= nx*ny)
        return p_field;

    int c = field[pos];
    char **prev9cells = (c == ALIVE) ? prev9cells_alive : prev9cells_dead;
    int num_cand = (c == ALIVE) ? NUM_CANDIDATES_9CELLS_ALIVE
                                : NUM_CANDIDATES_9CELLS_DEAD;

    // For all 3x3 cells pattern
    // whose center cell becomes the given field state.
    for (int i = 0; i < num_cand; i++) {
       
        // Check the canditate macthes with the existed cells.
        if (match9cells(p_field, nx, ny, prev9cells[i], pos) == UNMACHED)
            continue;

        // Copy of the previous field candidate.
        char *p_field_ = (char *)malloc(sizeof(char) * nx * ny);
        memcpy(p_field_, p_field, sizeof(char) * nx * ny);

        p_field_ = overwrite9cells(p_field_, nx, ny, prev9cells[i], pos); 

        // Find the previous pattern for the next cell.
        p_field_ = _prev_field(field, nx, ny, p_field_, pos+1);

        // No pattern found. Try with the next candidate.
        if (p_field_ == NULL)
            continue;

        // Found!!!
        // Return the first founded patter.
        // TODO: Search suitable patterns successively.
        free(p_field);
        return p_field_;
    }

    // All candidate is not suitable.
    free(p_field);
    return NULL;
}

// Get a field pattern that becomes to
// the given field pattern at the next step.
char *prev_field(const char *field, int nx, int ny)
{
    char *p_field = (char *)malloc(sizeof(char) * nx * ny); // TODO: free
    for (int i = 0; i < nx*ny; i++)
        p_field[i] = EMPTY;

    p_field = _prev_field(field, nx, ny, p_field, 0);

    return p_field;
}

// TODO
char *ansistor_field(const char *field, int nx, int ny, int num_back)
{
    return NULL;
}

int main(int argc, char *argv[])
{
    initialize();

    char field[] = {
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 1, 1, 0, 0, 0, 0, 0,
      0, 0, 1, 1, 0, 0, 0, 0,
      0, 1, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0
      };

    char *p_field = prev_field(field, 8, 8);

    if (p_field == NULL) {
        fprintf(stderr, "No previous field pattern was found.\n");
        return 1;
    }

    print_field(p_field, 8, 8);

    return 0;
}
