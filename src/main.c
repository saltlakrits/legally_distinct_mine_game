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
// * Flash surrounding unflagged if there aren't enough surrounding
// 	 flags
// * Time counter & print out time when player wins
// * Maybe save personal best to a file?

int main(int argc, char *argv[]) {
  // seed the rng
  srand(time(NULL));

  struct dimensions dims;

  struct notcurses_options nco = {.flags = NCOPTION_SUPPRESS_BANNERS};
  struct notcurses *nc = notcurses_init(&nco, NULL);
  struct ncplane *ncp = notcurses_stddim_yx(nc, &dims.LINES, &dims.COLS);
  struct ncinput nci;

	// Process CLI args
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
    dims.lines = dims.LINES - 3;
    dims.cols = dims.COLS / 3;
  } else {
    switch (argc) {
    case 1:
      // no args

      // drawing constraints
      // lines and cols should be set by user but have a default
      dims.lines = dims.LINES / 2;
      dims.cols = dims.COLS * 0.2;
      break;
    case 2:
      // NOTE: if arg can't be read as an int, it will default to 10,
      // which works fine

      // one arg (grid should be arg x arg)

      dims.lines = strtol(argv[1], NULL, 10);
      dims.lines = (dims.lines >= 10) ? dims.lines : 10;
      dims.cols = dims.lines;

      // check bounds
      dims.lines = (dims.lines <= dims.LINES - 3) ? dims.lines : dims.LINES - 3;
      dims.cols = (dims.cols <= dims.COLS / 3) ? dims.cols : dims.COLS / 3;
      break;
    case 3:
      // two args (grid should be arg1 x arg2)

      dims.lines = strtol(argv[1], NULL, 10);
      dims.cols = strtol(argv[2], NULL, 10);

      // check bounds
      dims.lines = (dims.lines <= dims.LINES - 3) ? dims.lines : dims.LINES - 3;
      dims.lines = (dims.lines >= 10) ? dims.lines : 10;
      dims.cols = (dims.cols <= dims.COLS / 3) ? dims.cols : dims.COLS / 3;
      dims.cols = (dims.cols >= 10) ? dims.cols : 10;

      break;
    }
  }

  dims.start_y = (dims.LINES - dims.lines) / 2;
  dims.start_x = (dims.COLS - dims.cols * X_MULT) / 2;

  // enables mice, the events are OR'd together (-> eventmask)
  notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);

  ncplane_erase(ncp);

  dummy_field(ncp, dims);
  notcurses_render(nc);

  // current tile, for easier access when looping through field
  struct tile *curr;

  // wait for user to click inside grid
  get_initial_tile(nc, &nci, dims);

  // quitting
  if (nci.id == 'q' || nci.id == 'Q' || nci.id == NCKEY_ESC) {
    notcurses_stop(nc);
    return 0;
  }

  // make a minefield
  struct tile(*field)[dims.lines][dims.cols] =
      NEW_MINEFIELD(dims.lines, dims.cols, nci.y - dims.start_y,
                    (nci.x - dims.start_x) / X_MULT);

	// this will be reused to pass various points to functions
  struct point pnt = {.y = nci.y - dims.start_y,
                      .x = (nci.x - dims.start_x) / X_MULT};

  // process initial click
  curr = &(*field)[nci.y - dims.start_y][(nci.x - dims.start_x) / X_MULT];
  curr->flags |= IS_UNCOVERED;
  if (curr->count == 0) {
    uncover_surrounding_tiles(field, pnt, dims);
  }

  // 20% of the tiles of any given grid are mines
  int mines = dims.lines * dims.cols * 0.2;
  int flags = 0;

  // print remaining # of mines
  ncplane_erase_region(ncp, dims.start_y - 1, 0, -1, dims.COLS);
  ncplane_set_bg_default(ncp);
  ncplane_set_fg_rgb(ncp, GREY);
  ncplane_printf_yx(ncp, dims.start_y - 1, dims.start_x, "Remaining mines: %d",
                    mines - flags);

  int has_lost = 0;
  int reinitialize = 0;

  // draw "actual" (instead of dummy) field now
  print_grid(ncp, field, &has_lost, dims);

	// MAIN LOOP
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
    if (nci.y < dims.start_y || nci.y >= dims.start_y + dims.lines ||
        nci.x < dims.start_x || nci.x >= dims.start_x + dims.cols * X_MULT) {
      // if not, just get new input
      continue;
    } else {
      // else we grab the tile as the current one to handle, just for ergonomics
      curr = &(*field)[nci.y - dims.start_y][(nci.x - dims.start_x) / X_MULT];
    }

    // if mouse1
    if (nci.id == NCKEY_BUTTON1 && nci.evtype == NCTYPE_RELEASE) {

      // coords easy to pass to functions
      pnt.y = nci.y - dims.start_y;
      pnt.x = (nci.x - dims.start_x) / X_MULT;

      // if tile is already uncovered, we want to check if there are
      // at least as many flags surrounding the tile as the number
      // of surrounding mines
      if (curr->flags & IS_UNCOVERED) {

        if (count_surrounding_flags(field, pnt, dims)) {

          uncover_surrounding_tiles(field, pnt, dims);
          print_grid(ncp, field, &has_lost, dims);
        }
      } else if (!(curr->flags & IS_FLAGGED)) {
        // make uncovered
        curr->flags |= IS_UNCOVERED;
        if (!(curr->flags & IS_BOMB) && curr->count == 0) {
          uncover_surrounding_tiles(field, pnt, dims);
        }
        print_grid(ncp, field, &has_lost, dims);
      }
    }
    if (nci.id == NCKEY_BUTTON3 && nci.evtype == NCTYPE_RELEASE) {

      // field coords for accurately choosing tile color to print
			int x_coord = (nci.x - dims.start_x) / X_MULT;
			int y_coord = nci.y - dims.start_y;

      int TILE = (y_coord + x_coord) % 2 ? LIGHT_TILE : DARK_TILE;
      ncplane_set_bg_rgb(ncp, TILE);

			// calculate actual x coord to print to
			x_coord = dims.start_x + x_coord * X_MULT;

      if (!(curr->flags & (IS_UNCOVERED | IS_FLAGGED))) {
        // toggle on flag
        flags++;
        ncplane_set_fg_rgb(ncp, BLACK);
        ncplane_putstr_yx(ncp, nci.y, x_coord, FLAG);
      } else if (curr->flags & IS_FLAGGED) {
        // toggle off flag
        flags--;
        ncplane_putstr_yx(ncp, nci.y, x_coord, BLANK);
      }

			// regardless, the flag on the tile struct should be toggled
      curr->flags ^= IS_FLAGGED;

      ncplane_set_bg_default(ncp);
      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_erase_region(ncp, dims.start_y - 1, dims.start_x, -1, dims.COLS);
      ncplane_printf_yx(ncp, dims.start_y - 1, dims.start_x,
                        "Remaining mines: %d", mines - flags);
    }

    if (has_lost) {
      ncplane_set_fg_rgb(ncp, RED);
      ncplane_set_bg_default(ncp);
      ncplane_erase_region(ncp, dims.start_y - 1, dims.start_x, -1, dims.COLS);
      ncplane_putstr_yx(ncp, dims.start_y - 1, dims.start_x, "You lost :(");
      notcurses_render(nc);

      notcurses_get_blocking(nc, NULL);
      notcurses_get_blocking(nc, NULL);

      reinitialize = prompt_reinit(nc, ncp, &nci, dims);

      if (!reinitialize) {
        notcurses_stop(nc);
        return 0;
      }
    }

    if (nci.evtype == NCTYPE_RELEASE) {
      // only check if we won on key release, to avoid running it twice
      // per keypress
      // things only change on key release anyway!
      if (has_won(field, dims)) {
        ncplane_set_fg_rgb(ncp, PURPLE);
        ncplane_set_bg_default(ncp);
        ncplane_erase_region(ncp, dims.start_y - 1, dims.start_x, -1,
                             dims.COLS);
        ncplane_putstr_yx(ncp, dims.start_y - 1, dims.start_x,
                          "You won! Time: NYINYI");
        notcurses_render(nc);

        notcurses_get_blocking(nc, NULL);
        notcurses_get_blocking(nc, NULL);

        reinitialize = prompt_reinit(nc, ncp, &nci, dims);

        if (!reinitialize) {
          notcurses_stop(nc);
          return 0;
        }
      }
    }

    if (reinitialize) {
      // erase whole screen and basically repeat initialization from
      // the top

      ncplane_erase_region(ncp, 0, 0, dims.LINES - 1, dims.COLS - 1);
      dummy_field(ncp, dims);
      notcurses_render(nc);

      // wait for user to click inside grid
      get_initial_tile(nc, &nci, dims);

      // quitting
      if (nci.id == 'q' || nci.id == 'Q' || nci.id == NCKEY_ESC) {
        notcurses_stop(nc);
        return 0;
      }

      free(field);
      field = NEW_MINEFIELD(dims.lines, dims.cols, nci.y - dims.start_y,
                            (nci.x - dims.start_x) / X_MULT);

      // process initial click
      curr = &(*field)[nci.y - dims.start_y][(nci.x - dims.start_x) / X_MULT];
      curr->flags |= IS_UNCOVERED;
      if (curr->count == 0) {
        pnt.y = nci.y - dims.start_y;
        pnt.x = (nci.x - dims.start_x) / X_MULT;
        uncover_surrounding_tiles(field, pnt, dims);
      }

      // print remaining # of mines
      ncplane_erase_region(ncp, dims.start_y - 1, 0, -1, dims.COLS);
      ncplane_set_bg_default(ncp);
      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_printf_yx(ncp, dims.start_y - 1, dims.start_x,
                        "Remaining mines: %d", mines - flags);

      // draw "actual" (instead of dummy) field now
      print_grid(ncp, field, &has_lost, dims);

      has_lost = 0;
      reinitialize = 0;
    }
  }

  // should never get here
  return 1;
}
