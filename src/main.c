#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>
#include <stdint.h>
#include <time.h>

#include "minefield.h"

#define X_MULT 3
#define Y_MULT 1

#define MIN(A, B) ((A < B) ? A : B)

#define LIGHT_MODE 0

#ifndef LIGHT_MODE
#define LIGHT_MODE 0
#endif

#if LIGHT_MODE == 1
#define GREY 0x404040
#define GREEN 0x47d147
#define PURPLE 0x8838ef
#define BLACK 0x000000

#define LIGHT_TILE 0xe6e6e6
#define DARK_TILE 0xcccccc
#else
#define GREY 0xe6e6e6
#define GREEN 0x7fff78
#define PURPLE 0x9a4efc
#define BLACK 0xe6e6e6

#define LIGHT_TILE 0x595959
#define DARK_TILE 0x404040
#endif

#define RED 0xff6b6b
#define ALARM 0xff3f2e
#define BLUE 0x00a2ff
#define ORANGE 0xffae00
#define LIME 0xb3ff00

#define BLANK "   "
#define COUNT " %d "
#define BOMB " \u25CF "
#define FLAG " \U0001F3F2 "

// TODO:
// * First click should give a bigger area (at least 3x3 tiles)
//   to work with

int count_color(uint8_t c) {
  switch (c) {
  case 1:
    return GREY;
  case 2:
    return GREEN;
  case 3:
    return BLUE;
  case 4:
    return PURPLE;
  case 5:
    return ORANGE;
  case 6:
    return RED;
  case 7:
    return ALARM;
  case 8:
    return LIME;
  default:
    return 0;
  }
}

int main() {
  // seed the rng
  srand(time(NULL));

  unsigned LINES, COLS;

  struct notcurses_options nco = {.flags = NCOPTION_SUPPRESS_BANNERS};
  struct notcurses *nc = notcurses_init(&nco, NULL);
  struct ncplane *ncp = notcurses_stddim_yx(nc, &LINES, &COLS);
  struct ncinput nci;

  // enables mice, the events are OR'd together (-> eventmask)
  notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);

  ncplane_erase(ncp);

  // drawing constraints
  // lines and cols should be set by user
  int lines = LINES / 2;
  int cols = COLS * 0.2;
  int start_y = (LINES - lines) / 2;
  int start_x = (COLS - cols * X_MULT) / 2;

  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < cols; x++) {
      if ((x + y) % 2) {
        ncplane_set_bg_rgb(ncp, LIGHT_TILE);
      } else {
        ncplane_set_bg_rgb(ncp, DARK_TILE);
      }
      ncplane_putstr_yx(ncp, y + start_y, x * X_MULT + start_x, BLANK);
    }
  }
  ncplane_set_fg_rgb(ncp, GREY);
  ncplane_set_bg_default(ncp);
  ncplane_putstr_yx(ncp, start_y - 1, COLS / 2 - 18,
                    "To start, click a tile on the field!");
  notcurses_render(nc);

  // current tile, for easier access when looping through field
  struct tile *curr;
  while (1) {
    notcurses_get_blocking(nc, &nci);
    if (nci.y < start_y || nci.y > start_y + lines || nci.x < start_x ||
        nci.x > start_x + cols * X_MULT) {
      continue;
    }
    if (nci.evtype == NCTYPE_RELEASE) {
      break;
    }
  }
  struct tile(*field)[lines][cols] =
      NEW_MINEFIELD(lines, cols, nci.y - start_y, (nci.x - start_x) / X_MULT);

  curr = &(*field)[nci.y - start_y][(nci.x - start_x) / X_MULT];
  curr->flags |= IS_UNCOVERED;
  if (curr->count == 0) {
    uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                              (nci.x - start_x) / X_MULT);
  }
  // 20% of the tiles of any given grid are mines
  int mines = lines * cols * 0.2;
  int flags = 0;

  ncplane_erase_region(ncp, start_y - 1, 0, -1, COLS);
  ncplane_set_bg_default(ncp);
  ncplane_set_fg_rgb(ncp, GREY);
  ncplane_printf_yx(ncp, start_y - 1, start_x, "Remaining mines: %d",
                    mines - flags);
  // draw field now

  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < cols; x++) {
      curr = &(*field)[y][x];
      if (curr->flags & IS_UNCOVERED) {
        ncplane_set_fg_rgb(ncp, count_color(curr->count));
        ncplane_set_bg_default(ncp);
        if (curr->count) {
          ncplane_printf_yx(ncp, y + start_y, x * X_MULT + start_x, COUNT,
                            curr->count);
        } else {
          ncplane_printf_yx(ncp, y + start_y, x * X_MULT + start_x, BLANK);
        }
      } else {
        if ((x + y) % 2) {
          ncplane_set_bg_rgb(ncp, LIGHT_TILE);
        } else {
          ncplane_set_bg_rgb(ncp, DARK_TILE);
        }
        ncplane_putstr_yx(ncp, y + start_y, x * X_MULT + start_x, BLANK);
      }
    }
  }
  int frames_drawn = 0;
  int has_lost = 0;

  while (1) {
    notcurses_render(nc);

    notcurses_get_blocking(nc, &nci);
    if (nci.id == 'q' || nci.id == 'Q' || nci.id == NCKEY_ESC) {
      notcurses_stop(nc);
      return 0;
    }

    if (nci.y < start_y || nci.y >= start_y + lines || nci.x < start_x ||
        nci.x >= start_x + cols * X_MULT) {
      continue;
    } else {
      // we grab the tile as the current one to handle, just for ergonomics
      curr = &(*field)[nci.y - start_y][(nci.x - start_x) / X_MULT];
    }

    if (nci.id == NCKEY_BUTTON1 && nci.evtype == NCTYPE_RELEASE) {

      // if tile is already uncovered, we want to check if there are
      // at least as many flags surrounding the tile as the number
      // of surrounding mines
      if (curr->flags & IS_UNCOVERED) {
        if (count_surrounding_flags(field, lines, cols, nci.y - start_y,
                                    (nci.x - start_x) / X_MULT)) {

          uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                                    (nci.x - start_x) / X_MULT);

          struct tile *inner_curr;
          for (int y = 0; y < lines; y++) {
            for (int x = 0; x < cols; x++) {
              inner_curr = &(*field)[y][x];
              if (inner_curr->flags & IS_UNCOVERED) {
                if (inner_curr->flags & IS_BOMB) {
                  has_lost = 1;
                  ncplane_set_fg_rgb(ncp, ALARM);
                  ncplane_set_bg_default(ncp);
                  ncplane_printf_yx(ncp, y + start_y, x * X_MULT + start_x,
                                    BOMB);
                } else {
                  ncplane_set_fg_rgb(ncp, count_color(inner_curr->count));
                  ncplane_set_bg_default(ncp);
                  if (inner_curr->count != 0) {
                    ncplane_printf_yx(ncp, y + start_y, x * X_MULT + start_x,
                                      COUNT, inner_curr->count);
                  } else {
                    ncplane_putstr_yx(ncp, y + start_y, x * X_MULT + start_x,
                                      BLANK);
                  }
                }
              }
            }
          }
          notcurses_render(nc);
        }
      } else if (!(curr->flags & IS_FLAGGED)) {
        curr->flags |= IS_UNCOVERED;

        if (curr->flags & IS_BOMB) {
          has_lost = 1;
          ncplane_set_fg_rgb(ncp, ALARM);
          ncplane_set_bg_default(ncp);
          int x_coord = (nci.x - start_x) / X_MULT;
          x_coord = x_coord * X_MULT + start_x;
          ncplane_printf_yx(ncp, nci.y, x_coord, BOMB);
          notcurses_render(nc);
        } else if (curr->count == 0) {
          uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                                    (nci.x - start_x) / X_MULT);
          struct tile *inner_curr;
          for (int y = 0; y < lines; y++) {
            for (int x = 0; x < cols; x++) {
              inner_curr = &(*field)[y][x];
              if (inner_curr->flags & IS_UNCOVERED) {
                ncplane_set_fg_rgb(ncp, count_color(inner_curr->count));
                ncplane_set_bg_default(ncp);
                if (inner_curr->count != 0) {
                  ncplane_printf_yx(ncp, y + start_y, x * X_MULT + start_x,
                                    COUNT, inner_curr->count);
                } else {
                  ncplane_putstr_yx(ncp, y + start_y, x * X_MULT + start_x,
                                    BLANK);
                }
              }
            }
          }
          notcurses_render(nc);
        } else {
          ncplane_set_bg_default(ncp);
          ncplane_set_fg_rgb(ncp, count_color(curr->count));
          // the following looks really stupid (and probably is), but
          // we rely on the integer division in the first calculation
          int x_coord = (nci.x - start_x) / X_MULT; // this is field coord
          x_coord = x_coord * X_MULT + start_x;
          ncplane_printf_yx(ncp, nci.y, x_coord, COUNT, curr->count);
          notcurses_render(nc);
        }
      }
    }
    if (nci.id == NCKEY_BUTTON3 && nci.evtype == NCTYPE_RELEASE) {
      int x_coord = (nci.x - start_x) / X_MULT;
      x_coord = x_coord * X_MULT + start_x;
      if ((nci.y + x_coord) % 2) {
        ncplane_set_bg_rgb(ncp, LIGHT_TILE);
      } else {
        ncplane_set_bg_rgb(ncp, DARK_TILE);
      }

      if (!(curr->flags & IS_UNCOVERED) && !(curr->flags & IS_FLAGGED)) {
        flags++;
        curr->flags ^= IS_FLAGGED;
        ncplane_set_fg_rgb(ncp, BLACK);
        // need to find out if this x+y is even or odd to color square
        ncplane_putstr_yx(ncp, nci.y, x_coord, FLAG);
      } else if (curr->flags & IS_FLAGGED) {
        flags--;
        curr->flags ^= IS_FLAGGED;
        ncplane_putstr_yx(ncp, nci.y, x_coord, BLANK);
      }
      ncplane_set_bg_default(ncp);
      ncplane_set_fg_rgb(ncp, GREY);
      // string is max 19 chars, possibly only 18
      ncplane_erase_region(ncp, start_y - 1, start_x, -1, COLS);
      ncplane_printf_yx(ncp, start_y - 1, start_x, "Remaining mines: %d",
                        mines - flags);
      notcurses_render(nc);
    }
    if (has_lost) {
      // TODO: Prompt the user if they want to play again!
      // basically just reinitialize the game and start over loop

      ncplane_set_fg_rgb(ncp, RED);
      ncplane_set_bg_default(ncp);
      ncplane_erase_region(ncp, start_y - 1, start_x, -1, COLS);
      ncplane_putstr_yx(ncp, start_y - 1, start_x, "You lost :(");
      notcurses_render(nc);

      // two blocks to ensure we don't just exit on mouse1 release
      notcurses_get_blocking(nc, NULL);
      notcurses_get_blocking(nc, NULL);
      notcurses_stop(nc);
      return 0;
    }
    if (nci.evtype == NCTYPE_RELEASE) {
      // only check if we won on key release, to avoid running it twice
      // per keypress
      // things only change on key release anyway!
      if (has_won(field, lines, cols)) {
        // TODO: Prompt the user if they want to play again!
        ncplane_set_fg_rgb(ncp, PURPLE);
        ncplane_set_bg_default(ncp);
        ncplane_erase_region(ncp, start_y - 1, start_x, -1, COLS);
        ncplane_putstr_yx(ncp, start_y - 1, start_x, "You won! Time: NYINYI");
        notcurses_render(nc);

        notcurses_get_blocking(nc, NULL);
        notcurses_get_blocking(nc, NULL);

        notcurses_stop(nc);
        return 0;
      }
    }
  }
  // should never get here
  return 1;
}
