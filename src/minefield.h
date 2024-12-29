#pragma once

#include <stdint.h>

// for tile flags
#define IS_UNCOVERED (1 << 0)
#define IS_BOMB (1 << 1)
#define IS_FLAGGED (1 << 2)

struct tile {
  uint8_t flags;
  uint8_t count;
};

// whenever we easily want to pass some coords,
// use this
struct point {
	int x;
	int y;
};

// we constantly pass around these variables, so let's keep
// them together
struct dims {
	const unsigned LINES;
	const unsigned COLS;
	int lines;
	int cols;
	int start_y;
	int start_x;
};

// don't call directly
void *new_minefield(int lines, int cols, int starty, int startx);

// wrapper macro
#define NEW_MINEFIELD(LINES, COLS, STARTY, STARTX)                             \
  ((struct tile(*)[LINES][COLS])new_minefield(LINES, COLS, STARTY, STARTX))

void uncover_surrounding_tiles(void *field, int lines, int cols, int y_coord,
                               int x_coord);
int count_surrounding_flags(void *field, int lines, int cols, int y_coord,
                            int x_coord);
int has_won(void *field, int lines, int cols);
void print_grid(struct ncplane *ncp, void *field, int *has_lost, int lines,
                int cols, int start_y, int start_x);
