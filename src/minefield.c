#include <stdlib.h>

#include "minefield.h"

const int rel_coords[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                              {0, 1},   {1, -1}, {1, 0},  {1, 1}};

// dimensions: lines, cols
// first click: y, x
void *new_minefield(int lines, int cols, int starty, int startx) {
  // decide minecount from lines x cols
  int tiles = lines * cols;
  int minecount = tiles * 0.2; // 20% mines on any given field

  // 0 for no neighboring mines
  // 1-8 for neighboring minecount
  // 9 for mine
  struct tile(*field)[lines][cols] = malloc(sizeof *field);

  // initialize
  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < cols; x++) {
      (*field)[y][x].flags = 0;
      (*field)[y][x].count = 0;
    }
  }

  int mines_to_place = minecount;

  // i think, theoretically, this could run forever
  // but that's so unlikely i'll just ignore it
  while (mines_to_place) {
    int y = rand() % (lines - 1);
    int x = rand() % (cols - 1);
    // will ensure first click is not a mine, but won't ensure
    // we always get a bigger empty area on first click
    // TODO: implement that
    if (y == starty || x == startx || ((*field)[y][x].flags & IS_BOMB)) {
      continue;
    }

    (*field)[y][x].flags |= IS_BOMB;
    mines_to_place--;
  }

  int rel_y, rel_x;

  // now we will find the neighbors and set the count, then we are done
  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < cols; x++) {

      // if this tile isn't a bomb, we count neighboring bombs
      if ((*field)[y][x].flags != IS_BOMB) {

        for (int i = 0; i < 8; i++) {
          rel_y = y + rel_coords[i][0];
          rel_x = x + rel_coords[i][1];
          // out of bounds check
          if (rel_y < 0 || rel_y >= lines || rel_x < 0 || rel_x >= cols) {
            continue;
          }
          // tile is within bounds
          if ((*field)[rel_y][rel_x].flags & IS_BOMB) {
            (*field)[y][x].count++;
          }
        }
      }
    }
  }

  return field;
}

void uncover_surrounding_tiles(void *field, int lines, int cols, int y_coord,
                               int x_coord) {
  struct tile(*local_field)[lines][cols] = field;

  // we want to reveal all surrounding tiles that aren't flagged
  // rel_coords[8][2] helps finding relative coords

  struct tile *curr;
  int y_mod, x_mod;

  for (int i = 0; i < 8; i++) {
    y_mod = y_coord + rel_coords[i][0];
    x_mod = x_coord + rel_coords[i][1];

    if (y_mod < 0 || y_mod >= lines || x_mod < 0 || x_mod >= cols) {
      continue;
    } else {
      // set curr to tile we are looking at for readability
      curr = &(*local_field)[y_mod][x_mod];
    }

    if (!(curr->flags & IS_UNCOVERED) && !(curr->flags & IS_FLAGGED)) {
      // uncover it, if count in it is 0 -> call this function on that tile too
      // if bomb -> lose, maybe return non-zero for lost?
      // and return 0 if no bomb uncovered
      curr->flags ^= IS_UNCOVERED;
      if (curr->count == 0) {
        uncover_surrounding_tiles(field, lines, cols, y_mod, x_mod);
      }
    }
  }
}

int count_surrounding_flags(void *field, int lines, int cols, int y_coord,
                            int x_coord) {
  struct tile(*local_field)[lines][cols] = field;
  int expected = (*local_field)[y_coord][x_coord].count;
  int count = 0;

  struct tile *curr;
  int y_mod, x_mod;
  for (int i = 0; i < 8; i++) {
    y_mod = y_coord + rel_coords[i][0];
    x_mod = x_coord + rel_coords[i][1];

    if (y_mod < 0 || y_mod >= lines || x_mod < 0 || x_mod >= cols) {
      continue;
    } else {
      // set curr to tile we are looking at for readability
      curr = &(*local_field)[y_mod][x_mod];
    }
		if (curr->flags & IS_FLAGGED) {
			count++;
		}
  }
  return (expected == count);
}

int has_won(void *field, int lines, int cols) {
	struct tile (*local_field)[lines][cols] = field;

	struct tile *curr;
	for (int y = 0; y < lines; y++) {
		for (int x = 0; x < cols; x++) {
			curr = &(*local_field)[y][x];

			if (!(curr->flags & IS_BOMB) && !(curr->flags & IS_UNCOVERED)) {
				return 0;
			}
		}
	}
	return 1;
}
