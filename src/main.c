#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>
#include <stdint.h>
#include <time.h>

#include "minefield.h"

#define X_MULT 3
#define Y_MULT 2

#define MIN(A, B) ((A < B) ? A : B)

#define RED 0xff6b6b
#define PURPLE 0x8838ef

// TODO:
// * Solve the lopsided grid?
// * The field is too wide
// * When uncovering a 0, it should not be written out,
// 	 and it should uncover all surrounding tiles (and that
// 	 should propagate to new 0s)
// * Win condition when all mines are flagged
// * First click should give a bigger area (at least 3x3 tiles)
//   to work with

int main() {
  // seed the rng
  srand(time(NULL));

  unsigned LINES, COLS;

  struct notcurses *nc = notcurses_init(NULL, NULL);
  struct ncplane *ncp = notcurses_stddim_yx(nc, &LINES, &COLS);
  struct ncinput nci;

  // enables mice, the events are OR'd together (-> eventmask)
  notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);

  // uint32_t fg = NCCHANNEL_INITIALIZER(219, 113, 255);
  // uint32_t fg = NCCHANNEL_INITIALIZER(136, 56, 239);
  // uint32_t bg = NCCHANNEL_INITIALIZER(0, 0, 0);
  // uint64_t base_channels = ncchannels_combine(fg, bg);

  // 0x8839ef for covered tiles == rgb(136, 57, 239)


  ncplane_erase(ncp);

  // drawing constraints
  // int lines = LINES;
  // int cols = COLS / X_MULT;
  int lines = 10;
  int cols = 10;
  int start_y = (LINES - lines) / 2;
  int start_x = (COLS - cols) / 2;

  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < cols; x++) {
      ncplane_set_bg_rgb(ncp, PURPLE);
      ncplane_putstr_yx(ncp, y + start_y, x + start_x, " ");
    }
  }
  ncplane_set_fg_rgb(ncp, PURPLE);
  ncplane_set_bg_default(ncp);
  ncplane_putstr_yx(ncp, 0, 0, "To start, click a tile on the field!");
  notcurses_render(nc);

  // current tile, for easier access when looping through field
  struct tile *curr;
  // TODO: Get input here, get y, x of mouse click -> send to NEW_MINEFIELD
  // render "dummy" minefield to click on
  while (1) {
    notcurses_get_blocking(nc, &nci);
    if (nci.y < start_y || nci.y > start_y + lines || nci.x < start_x ||
        nci.x > start_x + cols) {
      continue;
    }
    if (nci.evtype == NCTYPE_RELEASE) {
      break;
    }
  }
  struct tile(*field)[lines][cols] =
      NEW_MINEFIELD(lines, cols, nci.y - start_y, nci.x - start_y);
  curr = &(*field)[nci.y - start_y][nci.x - start_x];
  curr->flags |= IS_UNCOVERED;
  if (curr->count == 0) {
    uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                              nci.x - start_x);
  }
  // recall the .flags are IS_BOMB, IS_FLAGGED, IS_UNCOVERED
  uint8_t mines = lines * cols * 0.2;

  // draw field now

  for (int y = 0; y < lines; y++) {
    for (int x = 0; x < cols; x++) {
      curr = &(*field)[y][x];
      if (curr->flags & IS_UNCOVERED) {
				ncplane_set_fg_rgb(ncp, PURPLE);
        ncplane_set_bg_default(ncp);
        ncplane_printf_yx(ncp, y + start_y, x + start_x, "%d", curr->count);
      } else {
        ncplane_set_bg_rgb(ncp, PURPLE);
        ncplane_putstr_yx(ncp, y + start_y, x + start_x, " ");
      }
    }
  }
  int frames_drawn = 0;

  // TODO: Write some get_surrounding function to easily check surrounding
  // tiles?

  int has_lost = 0;

  while (1) {
    frames_drawn++;
    ncplane_set_fg_rgb(ncp, PURPLE);
    ncplane_set_bg_default(ncp);
    ncplane_printf_yx(ncp, 1, 0, "Buttons recorded: %d", frames_drawn);

    notcurses_render(nc);

    notcurses_get_blocking(nc, &nci);
    if (nci.id == 'q' || nci.id == 'Q' || nci.id == NCKEY_ESC) {
      notcurses_stop(nc);
      return 0;
    }

    if (nci.y < start_y || nci.y >= start_y + lines || nci.x < start_x ||
        nci.x >= start_x + cols) {
      continue;
    } else {
      // we grab the tile as the current one to handle, just for ergonomics
      curr = &(*field)[nci.y - start_y][nci.x - start_x];
    }

    if (nci.id == NCKEY_BUTTON1 && nci.evtype == NCTYPE_RELEASE) {
      if (curr->flags & IS_UNCOVERED) {
        if (count_surrounding_flags(field, lines, cols, nci.y - start_y,
                                    nci.x - start_x)) {

          uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                                    nci.x - start_x);
          struct tile *inner_curr;
          for (int y = 0; y < lines; y++) {
            for (int x = 0; x < cols; x++) {
              inner_curr = &(*field)[y][x];
              if (inner_curr->flags & IS_UNCOVERED) {
                if (inner_curr->flags & IS_BOMB) {
                  has_lost = 1;
                  ncplane_set_fg_rgb(ncp, RED);
                  ncplane_set_bg_default(ncp);
                  ncplane_printf_yx(ncp, y + start_y, x + start_x, "B");
                } else {
                  ncplane_set_fg_rgb(ncp, PURPLE);
                  ncplane_set_bg_default(ncp);
                  if (inner_curr->count != 0) {
                    ncplane_printf_yx(ncp, y + start_y, x + start_x, "%d",
                                      inner_curr->count);
                    notcurses_render(nc);
                  } else {
                    ncplane_putstr_yx(ncp, y + start_y, x + start_x, " ");
                    notcurses_render(nc);
                  }
                }
              }
            }
          }
        } else {
        }
      } else if (!(curr->flags & IS_FLAGGED)) {
        curr->flags |= IS_UNCOVERED; // uncover it if covered
                                     // TODO: if count == 0, uncover
                                     // surrounding, this should propagate!
        if (curr->flags & IS_BOMB) {
          has_lost = 1;
          ncplane_set_fg_rgb8(ncp, 255, 107, 107);
          ncplane_set_bg_default(ncp);
          // TODO: Try fullwidth B
          ncplane_printf_yx(ncp, nci.y, nci.x, "B");
          notcurses_render(nc);
        } else if (curr->count == 0) {
          uncover_surrounding_tiles(field, lines, cols, nci.y - start_y,
                                    nci.x - start_x);
          struct tile *inner_curr;
          for (int y = 0; y < lines; y++) {
            for (int x = 0; x < cols; x++) {
              inner_curr = &(*field)[y][x];
              if (inner_curr->flags & IS_UNCOVERED) {
                ncplane_set_fg_rgb8(ncp, 136, 56, 239);
                ncplane_set_bg_default(ncp);
                if (inner_curr->count != 0) {
                  ncplane_printf_yx(ncp, y + start_y, x + start_x, "%d",
                                    inner_curr->count);
                  notcurses_render(nc);
                } else {
                  ncplane_putstr_yx(ncp, y + start_y, x + start_x, " ");
                  notcurses_render(nc);
                }
              }
            }
          }
        } else {
          ncplane_set_bg_default(ncp);
          ncplane_set_fg_rgb8(ncp, 136, 56, 239);
          // TODO: set foreground color according to count
          // TODO: try using fullwidth numbers and using 2 cols for each tile
          ncplane_printf_yx(ncp, nci.y, nci.x, "%d", curr->count);
          notcurses_render(nc);
        }
      }
    }
    if (nci.id == NCKEY_BUTTON3 && nci.evtype == NCTYPE_RELEASE) {
      if (!(curr->flags & IS_UNCOVERED) && !(curr->flags & IS_FLAGGED)) {
        curr->flags ^= IS_FLAGGED;
        ncplane_set_fg_rgb8(ncp, 255, 107, 107);
        ncplane_set_bg_rgb8(ncp, 136, 56, 239);
        ncplane_putstr_yx(ncp, nci.y, nci.x, "F");
        notcurses_render(nc);
      } else if (curr->flags & IS_FLAGGED) {
        curr->flags ^= IS_FLAGGED;
        ncplane_set_bg_rgb8(ncp, 136, 56, 239);
        ncplane_putstr_yx(ncp, nci.y, nci.x, " ");
        notcurses_render(nc);
      }
    }
    if (has_lost) {
      // better but not ideal, works for now
      // TODO: give option to start a new game?
      // basically just reinitialize the game and start over loop

      // two blocks to ensure we don't just exit on mouse1 release
      notcurses_get_blocking(nc, NULL);
      notcurses_get_blocking(nc, NULL);
      notcurses_stop(nc);
      return 0;
    }
  }
  // should never get here
  return 1;
}
