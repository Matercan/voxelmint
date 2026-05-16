#ifndef __WL_DRAW_H
#define __WL_DRAW_H

#include "client.h"

#include <stdint.h>

#pragma once

/* A rectangle, from hence we can damage the surface */
typedef struct {
  /* X and Y positions of the rect */
  int32_t x, y;
  /* Length and Width of the rect */
  int32_t l, w;
} rect;

/* A color that can be rendered to the screen */
typedef struct {
  uint8_t r, g, b, a;
} color;

int clear(struct sd_state *);
int fill_rect(struct sd_state *, rect, color);
int draw_state(struct sd_state *, unsigned char *, size_t); 

void fill_transparency_black(struct sd_state *);
void rgba_to_argb(unsigned char *, size_t, size_t);

#endif
