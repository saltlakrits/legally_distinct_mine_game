#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>
#include <stdint.h>
#include <time.h>

#include "minefield.h"

#define X_MULT 3
#define Y_MULT 1

#define MAX(A, B) ((A > B) ? A : B)
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

#define PROMPT_BG LIGHT_TILE
#else
#define GREY 0xe6e6e6
#define GREEN 0x7fff78
#define PURPLE 0x9a4efc
#define BLACK 0xe6e6e6

#define LIGHT_TILE 0x595959
#define DARK_TILE 0x404040

#define PROMPT_BG 0x303030
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

void dummy_field(struct ncplane *ncp, int lines, int cols, int start_y,
                 int start_x) {
  // draw initial "dummy" grid
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

  // print some instructions
  ncplane_set_fg_rgb(ncp, GREY);
  ncplane_set_bg_default(ncp);
  ncplane_putstr_yx(ncp, start_y - 1, start_x,
                    "To start, click a tile on the field!");
}

void print_grid(struct ncplane *ncp, void *field, int lines, int cols,
                int start_y, int start_x) {
  struct tile(*local_field)[lines][cols] = field;
  struct tile *curr;

  // draw "actual" (instead of dummy) field now
  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < cols; x++) {

      curr = &(*local_field)[y][x];
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
        // if not uncovered
        if ((x + y) % 2) {
          ncplane_set_bg_rgb(ncp, LIGHT_TILE);
        } else {
          ncplane_set_bg_rgb(ncp, DARK_TILE);
        }
        ncplane_putstr_yx(ncp, y + start_y, x * X_MULT + start_x, BLANK);
      }
    }
  }
}

void get_initial_tile(struct notcurses *nc, struct ncinput *nci, int lines,
                      int cols, int start_y, int start_x) {
  while (1) {
    notcurses_get_blocking(nc, nci);
    if (nci->id == 'q' || nci->id == 'Q' || nci->id == NCKEY_ESC) {
      break;
    }
    if (nci->y < start_y || nci->y > start_y + lines || nci->x < start_x ||
        nci->x > start_x + cols * X_MULT) {
      continue;
    }
    if (nci->evtype == NCTYPE_RELEASE) {
      break;
    }
  }
}

int prompt_reinit(struct notcurses *nc, struct ncplane *ncp,
                  struct ncinput *nci, int lines, int cols, int start_y,
                  int start_x) {
  // prompt the user if they want to play again

  ncplane_set_fg_rgb(ncp, GREY);
  ncplane_set_bg_rgb(ncp, PROMPT_BG);

  char *bg_string = malloc((cols * 3 - 2) * sizeof *bg_string);
  memset(bg_string, ' ', cols * 3 - 2);

  ncplane_putstr_yx(ncp, start_y + 1, start_x + 1, bg_string);
  ncplane_putstr_yx(ncp, start_y + 2, start_x + 1, bg_string);
  ncplane_putstr_yx(ncp, start_y + 3, start_x + 1, bg_string);
  ncplane_putstr_yx(ncp, start_y + 4, start_x + 1, bg_string);
  ncplane_putstr_yx(ncp, start_y + 5, start_x + 1, bg_string);

  ncplane_printf_yx(ncp, start_y + 2, start_x + cols * 3 * 0.5 - 5,
                    "Play again?");

  ncplane_set_bg_rgb(ncp, DARK_TILE);
  int offset = 2;

  int yes_start = (start_x + cols * 3 * 0.25) - offset;
  int yes_end = yes_start + 3 + offset * 2;
  int no_start = (start_x + cols * 3 * 0.75) - offset;
  int no_end = no_start + 2 + offset * 2;

  ncplane_printf_yx(ncp, start_y + 4, yes_start, "  Yes  ");
  ncplane_printf_yx(ncp, start_y + 4, no_start, "  No  ");

  notcurses_render(nc);

  notcurses_mice_enable(nc, NCMICE_MOVE_EVENT | NCMICE_BUTTON_EVENT);
  // user input loop
  while (1) {
    notcurses_get_blocking(nc, nci);
    if (nci->y == start_y + 4 && nci->x >= yes_start && nci->x <= yes_end) {
      ncplane_set_fg_rgb(ncp, 0x202020);
      ncplane_set_bg_rgb(ncp, BLUE);
      ncplane_printf_yx(ncp, start_y + 4, yes_start, "  Yes  ");

      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_set_bg_rgb(ncp, DARK_TILE);
      ncplane_printf_yx(ncp, start_y + 4, no_start, "  No  ");
    } else if (nci->y == start_y + 4 && nci->x >= no_start &&
               nci->x <= no_end) {
      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_set_bg_rgb(ncp, DARK_TILE);
      ncplane_printf_yx(ncp, start_y + 4, yes_start, "  Yes  ");

      ncplane_set_fg_rgb(ncp, 0x202020);
      ncplane_set_bg_rgb(ncp, BLUE);
      ncplane_printf_yx(ncp, start_y + 4, no_start, "  No  ");
    } else {
      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_set_bg_rgb(ncp, DARK_TILE);
      ncplane_printf_yx(ncp, start_y + 4, yes_start, "  Yes  ");
      ncplane_printf_yx(ncp, start_y + 4, no_start, "  No  ");
    }
    if (nci->id == NCKEY_BUTTON1 && nci->evtype == NCTYPE_RELEASE) {
      if (nci->y == start_y + 4 && nci->x >= yes_start && nci->x <= yes_end) {

        free(bg_string);
        notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);
        return 1;
      } else if (nci->y == start_y + 4 && nci->x >= no_start &&
                 nci->x <= no_end) {

        free(bg_string);
        notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);
        return 0;
      }
    }
    notcurses_render(nc);
  }
}

int main(int argc, char *argv[]) {
  // seed the rng
  srand(time(NULL));

  unsigned LINES, COLS;

  struct notcurses_options nco = {.flags = NCOPTION_SUPPRESS_BANNERS};
  struct notcurses *nc = notcurses_init(&nco, NULL);
  struct ncplane *ncp = notcurses_stddim_yx(nc, &LINES, &COLS);
  struct ncinput nci;

  if (argc > 3) {
    notcurses_stop(nc);
    printf("\nUnrecognized argument count.\n");
    printf("\noptions:\n");
    printf("\t-h: prints this help\n");
    printf("\t<num>: gives you a num x num grid\n");
    printf("\t<n1> <n2>: gives you a n1 x n2 grid\n");
    printf("\t-m: makes the grid as large as possible for the terminal\n\n");
    return 1;
  }
  int lines, cols;

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      notcurses_stop(nc);

      printf("\nMinesweeper!\n\n\toptions:\n");
      printf("\t-h: prints this help\n");
      printf("\t<num>: gives you a num x num grid\n");
      printf("\t<n1> <n2>: gives you a n1 x n2 grid\n");
      printf("\t-m: makes the grid as large as possible for the terminal\n\n");
      return 0;
    }
  }

  if (argc == 2 && strcmp(argv[1], "-m") == 0) {
    lines = LINES - 3;
    cols = COLS / 3;
  } else {
    switch (argc) {
    case 1:
      // no args

      // drawing constraints
      // lines and cols should be set by user but have a default
      lines = LINES / 2;
      cols = COLS * 0.2;
      break;
    case 2:
      // NOTE: if arg can't be read as an int, it will default to 10,
      // which works fine

      // one arg (grid should be arg x arg)

      lines = strtol(argv[1], NULL, 10);
      lines = (lines >= 10) ? lines : 10;
      cols = lines;

      // check bounds
      lines = (lines <= LINES - 3) ? lines : LINES - 3;
      cols = (cols <= COLS / 3) ? cols : COLS / 3;
      break;
    case 3:
      // two args (grid should be arg1 x arg2)

      lines = strtol(argv[1], NULL, 10);
      cols = strtol(argv[2], NULL, 10);

      // check bounds
      lines = (lines <= LINES - 3) ? lines : LINES - 3;
      lines = (lines >= 10) ? lines : 10;
      cols = (cols <= COLS / 3) ? cols : COLS / 3;
      cols = (cols >= 10) ? cols : 10;

      break;
    }
  }

  int start_y = (LINES - lines) / 2;
  int start_x = (COLS - cols * X_MULT) / 2;

  // enables mice, the events are OR'd together (-> eventmask)
  notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);

  ncplane_erase(ncp);

  dummy_field(ncp, lines, cols, start_y, start_x);
  notcurses_render(nc);

  // current tile, for easier access when looping through field
  struct tile *curr;

  // wait for user to click inside grid
  get_initial_tile(nc, &nci, lines, cols, start_y, start_x);
  if (nci.id == 'q' || nci.id == 'Q' || nci.id == NCKEY_ESC) {
    notcurses_stop(nc);
    return 0;
  }

  // make a minefield
  struct tile(*field)[lines][cols] =
      NEW_MINEFIELD(lines, cols, nci.y - start_y, (nci.x - start_x) / X_MULT);

  // process initial click
  curr = &(*field)[nci.y - start_y][(nci.x - start_x) / X_MULT];
  curr->flags |= IS_UNCOVERED;
  if (curr->count == 0) {
    uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                              (nci.x - start_x) / X_MULT);
  }

  // 20% of the tiles of any given grid are mines
  int mines = lines * cols * 0.2;
  int flags = 0;

  // print remaining # of mines
  ncplane_erase_region(ncp, start_y - 1, 0, -1, COLS);
  ncplane_set_bg_default(ncp);
  ncplane_set_fg_rgb(ncp, GREY);
  ncplane_printf_yx(ncp, start_y - 1, start_x, "Remaining mines: %d",
                    mines - flags);

  // draw "actual" (instead of dummy) field now
  print_grid(ncp, field, lines, cols, start_y, start_x);

  int has_lost = 0;
  int reinitialize = 0;

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
        } else {
          ncplane_set_bg_default(ncp);
          ncplane_set_fg_rgb(ncp, count_color(curr->count));
          // the following looks really stupid (and probably is), but
          // we rely on the integer division in the first calculation
          int x_coord = (nci.x - start_x) / X_MULT; // this is field coord
          x_coord = x_coord * X_MULT + start_x;
          ncplane_printf_yx(ncp, nci.y, x_coord, COUNT, curr->count);
        }
      }
    }
    if (nci.id == NCKEY_BUTTON3 && nci.evtype == NCTYPE_RELEASE) {

      // calculate adjusted x coord
      int x_coord = (nci.x - start_x) / X_MULT;
      x_coord = x_coord * X_MULT + start_x;
      int grid_y = (nci.y - start_y);

      int TILE = (grid_y + x_coord) % 2 ? LIGHT_TILE : DARK_TILE;
      ncplane_set_bg_rgb(ncp, TILE);

      if (!(curr->flags & (IS_UNCOVERED | IS_FLAGGED))) {
        // toggle on flag
        flags++;
        ncplane_set_fg_rgb(ncp, BLACK);
        // need to find out if this x+y is even or odd to color square
        ncplane_putstr_yx(ncp, nci.y, x_coord, FLAG);
      } else if (curr->flags & IS_FLAGGED) {
        // toggle off flag
        flags--;
        ncplane_putstr_yx(ncp, nci.y, x_coord, BLANK);
      }

      curr->flags ^= IS_FLAGGED;

      ncplane_set_bg_default(ncp);
      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_erase_region(ncp, start_y - 1, start_x, -1, COLS);
      ncplane_printf_yx(ncp, start_y - 1, start_x, "Remaining mines: %d",
                        mines - flags);
    }

    if (has_lost) {
      // TODO: Prompt the user if they want to play again!
      // basically just reinitialize the game and start over loop?

      ncplane_set_fg_rgb(ncp, RED);
      ncplane_set_bg_default(ncp);
      ncplane_erase_region(ncp, start_y - 1, start_x, -1, COLS);
      ncplane_putstr_yx(ncp, start_y - 1, start_x, "You lost :(");
      notcurses_render(nc);

      // TODO: check if user wants to play again and set reinitialize if true
      notcurses_get_blocking(nc, NULL);
      notcurses_get_blocking(nc, NULL);

      reinitialize =
          prompt_reinit(nc, ncp, &nci, lines, cols, start_y, start_x);

      if (!reinitialize) {
        notcurses_stop(nc);
        return 0;
      }
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

        // TODO: check if user wants to play again and set reinitialize if true
        notcurses_get_blocking(nc, NULL);
        notcurses_get_blocking(nc, NULL);

        reinitialize =
            prompt_reinit(nc, ncp, &nci, lines, cols, start_y, start_x);

        if (!reinitialize) {
          notcurses_stop(nc);
          return 0;
        }
      }
    }

    if (reinitialize) {
      // erase whole screen and basically repeat initialization from
      // the top

      ncplane_erase_region(ncp, 0, 0, LINES - 1, COLS - 1);
      dummy_field(ncp, lines, cols, start_y, start_x);
      notcurses_render(nc);

      // wait for user to click inside grid
      get_initial_tile(nc, &nci, lines, cols, start_y, start_x);
      if (nci.id == 'q' || nci.id == 'Q' || nci.id == NCKEY_ESC) {
        notcurses_stop(nc);
        return 0;
      }

      free(field);
      field = NEW_MINEFIELD(lines, cols, nci.y - start_y,
                            (nci.x - start_x) / X_MULT);

      // process initial click
      curr = &(*field)[nci.y - start_y][(nci.x - start_x) / X_MULT];
      curr->flags |= IS_UNCOVERED;
      if (curr->count == 0) {
        uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                                  (nci.x - start_x) / X_MULT);
      }

      // print remaining # of mines
      ncplane_erase_region(ncp, start_y - 1, 0, -1, COLS);
      ncplane_set_bg_default(ncp);
      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_printf_yx(ncp, start_y - 1, start_x, "Remaining mines: %d",
                        mines - flags);

      print_grid(ncp, field, lines, cols, start_y, start_x);

      has_lost = 0;
      reinitialize = 0;
    }
  }

  // should never get here
  return 1;
}
