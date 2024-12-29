#pragma once

#define X_MULT 3

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



