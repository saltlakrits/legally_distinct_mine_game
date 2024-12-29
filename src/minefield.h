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
