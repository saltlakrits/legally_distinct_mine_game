#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>
#include <stdint.h>
#include <time.h>

#include "init.h"
#include "minefield.h"
#include "options.h"

#define MAX(A, B) ((A > B) ? A : B)
#define MIN(A, B) ((A < B) ? A : B)

// TODO:
// * First click should give a bigger area (at least 3x3 tiles)
//   to work with

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

  // quitting
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

  int has_lost = 0;
  int reinitialize = 0;

  // draw "actual" (instead of dummy) field now
  print_grid(ncp, field, &has_lost, lines, cols, start_y, start_x);

  while (1) {
    notcurses_render(nc);

    // get input
    notcurses_get_blocking(nc, &nci);

    // quitting
    if (nci.id == 'q' || nci.id == 'Q' || nci.id == NCKEY_ESC) {
      notcurses_stop(nc);
      return 0;
    }

    // if we didn't quit, check that we clicked inside grid
    if (nci.y < start_y || nci.y >= start_y + lines || nci.x < start_x ||
        nci.x >= start_x + cols * X_MULT) {
      // if not, just get new input
      continue;
    } else {
      // else we grab the tile as the current one to handle, just for ergonomics
      curr = &(*field)[nci.y - start_y][(nci.x - start_x) / X_MULT];
    }

    // if mouse1
    if (nci.id == NCKEY_BUTTON1 && nci.evtype == NCTYPE_RELEASE) {

      // if tile is already uncovered, we want to check if there are
      // at least as many flags surrounding the tile as the number
      // of surrounding mines
      if (curr->flags & IS_UNCOVERED) {
        if (count_surrounding_flags(field, lines, cols, nci.y - start_y,
                                    (nci.x - start_x) / X_MULT)) {

          uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                                    (nci.x - start_x) / X_MULT);

          print_grid(ncp, field, &has_lost, lines, cols, start_y, start_x);
        }
      } else if (!(curr->flags & IS_FLAGGED)) {
        // make uncovered
        curr->flags |= IS_UNCOVERED;
        if (!(curr->flags & IS_BOMB) && curr->count == 0) {
          uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                                    (nci.x - start_x) / X_MULT);
        }
        print_grid(ncp, field, &has_lost, lines, cols, start_y, start_x);
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

      // quitting
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

      // draw "actual" (instead of dummy) field now
      print_grid(ncp, field, &has_lost, lines, cols, start_y, start_x);

      has_lost = 0;
      reinitialize = 0;
    }
  }

  // should never get here
  return 1;
}
