#pragma once

#include <stdint.h>
#include "minefield.h"

int count_color(uint8_t c);
void dummy_field(struct ncplane *ncp, struct dimensions d);
void get_initial_tile(struct notcurses *nc, struct ncinput *nci,
                      struct dimensions d);
int prompt_reinit(struct notcurses *nc, struct ncplane *ncp,
                  struct ncinput *nci, struct dimensions d);
