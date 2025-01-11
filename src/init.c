#include <notcurses/notcurses.h>

#include "init.h"
#include "minefield.h"
#include "options.h"

typedef enum {
	grey = 0xe6e6e6,
} LightColors;

// consider making the colors an enum instead
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

void dummy_field(struct ncplane *ncp, struct dimensions d) {
  // draw initial "dummy" grid
  for (int y = 0; y < d.lines; y++) {
    for (int x = 0; x < d.cols; x++) {
      if ((x + y) % 2) {
        ncplane_set_bg_rgb(ncp, LIGHT_TILE);
      } else {
        ncplane_set_bg_rgb(ncp, DARK_TILE);
      }
      ncplane_putstr_yx(ncp, y + d.start_y, x * X_MULT + d.start_x, BLANK);
    }
  }

  // print some instructions
  ncplane_set_fg_rgb(ncp, GREY);
  ncplane_set_bg_default(ncp);
  ncplane_putstr_yx(ncp, d.start_y - 1, d.start_x,
                    "To start, click a tile on the field!");
}

void get_initial_tile(struct notcurses *nc, struct ncinput *nci,
                      struct dimensions d) {
  while (1) {
    notcurses_get_blocking(nc, nci);
    if (nci->id == 'q' || nci->id == 'Q' || nci->id == NCKEY_ESC) {
      break;
    }
    if (nci->y < d.start_y || nci->y > d.start_y + d.lines ||
        nci->x < d.start_x || nci->x > d.start_x + d.cols * X_MULT) {
      continue;
    }
    if (nci->evtype == NCTYPE_RELEASE) {
      break;
    }
  }
}

int prompt_reinit(struct notcurses *nc, struct ncplane *ncp,
                  struct ncinput *nci, struct dimensions d) {
  // prompt the user if they want to play again

  ncplane_set_fg_rgb(ncp, GREY);
  ncplane_set_bg_rgb(ncp, PROMPT_BG);

  char *bg_string = malloc((d.cols * 3 - 2) * sizeof *bg_string);
  memset(bg_string, ' ', d.cols * 3 - 2);

  ncplane_putstr_yx(ncp, d.start_y + 1, d.start_x + 1, bg_string);
  ncplane_putstr_yx(ncp, d.start_y + 2, d.start_x + 1, bg_string);
  ncplane_putstr_yx(ncp, d.start_y + 3, d.start_x + 1, bg_string);
  ncplane_putstr_yx(ncp, d.start_y + 4, d.start_x + 1, bg_string);
  ncplane_putstr_yx(ncp, d.start_y + 5, d.start_x + 1, bg_string);

  ncplane_printf_yx(ncp, d.start_y + 2, d.start_x + d.cols * 3 * 0.5 - 5,
                    "Play again?");

  ncplane_set_bg_rgb(ncp, DARK_TILE);
  int offset = 2;

  int yes_start = (d.start_x + d.cols * 3 * 0.25) - offset;
  int yes_end = yes_start + 3 + offset * 2;
  int no_start = (d.start_x + d.cols * 3 * 0.75) - offset;
  int no_end = no_start + 2 + offset * 2;

  ncplane_printf_yx(ncp, d.start_y + 4, yes_start, "  Yes  ");
  ncplane_printf_yx(ncp, d.start_y + 4, no_start, "  No  ");

  notcurses_render(nc);

  notcurses_mice_enable(nc, NCMICE_MOVE_EVENT | NCMICE_BUTTON_EVENT);
  // user input loop
  while (1) {
    notcurses_get_blocking(nc, nci);
    if (nci->y == d.start_y + 4 && nci->x >= yes_start && nci->x <= yes_end) {
      ncplane_set_fg_rgb(ncp, 0x202020);
      ncplane_set_bg_rgb(ncp, BLUE);
      ncplane_printf_yx(ncp, d.start_y + 4, yes_start, "  Yes  ");

      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_set_bg_rgb(ncp, DARK_TILE);
      ncplane_printf_yx(ncp, d.start_y + 4, no_start, "  No  ");
    } else if (nci->y == d.start_y + 4 && nci->x >= no_start &&
               nci->x <= no_end) {
      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_set_bg_rgb(ncp, DARK_TILE);
      ncplane_printf_yx(ncp, d.start_y + 4, yes_start, "  Yes  ");

      ncplane_set_fg_rgb(ncp, 0x202020);
      ncplane_set_bg_rgb(ncp, BLUE);
      ncplane_printf_yx(ncp, d.start_y + 4, no_start, "  No  ");
    } else {
      ncplane_set_fg_rgb(ncp, GREY);
      ncplane_set_bg_rgb(ncp, DARK_TILE);
      ncplane_printf_yx(ncp, d.start_y + 4, yes_start, "  Yes  ");
      ncplane_printf_yx(ncp, d.start_y + 4, no_start, "  No  ");
    }
    if (nci->id == NCKEY_BUTTON1 && nci->evtype == NCTYPE_RELEASE) {
      if (nci->y == d.start_y + 4 && nci->x >= yes_start && nci->x <= yes_end) {

        free(bg_string);
        notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);
        return 1;
      } else if (nci->y == d.start_y + 4 && nci->x >= no_start &&
                 nci->x <= no_end) {

        free(bg_string);
        notcurses_mice_enable(nc, NCMICE_BUTTON_EVENT);
        return 0;
      }
    }
    notcurses_render(nc);
  }
}
