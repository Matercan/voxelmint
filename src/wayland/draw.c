#include "draw.h"
#include "client.h"

#include <stddef.h>
#include <stdlib.h>
#include <wayland-client-protocol.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {}

void rgba_to_argb(unsigned char *data, size_t width, size_t height) {
  uint32_t *pixels = (uint32_t *)data;
  size_t pixel_count = width * height;

  for (size_t i = 0; i < pixel_count; i++) {
    uint32_t rgba = pixels[i];

    uint8_t r = (rgba >> 0) & 0xFF;
    uint8_t g = (rgba >> 8) & 0xFF;
    uint8_t b = (rgba >> 16) & 0xFF;
    uint8_t a = (rgba >> 24) & 0xFF;

    // Set all of the color behind the pixels to 0
    if (a == 0xff && r == 0 && g == 0 && b == 0) {
      a = 0;
    }

    pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
  }
}

int draw_state(struct sd_state *state, uint8_t *data, size_t size) {
  if (unlikely(!state->configured)) {
    printf("State not configured!\n");
    return -1;
  }

  int buffer_idx = get_next_buffer(state);
  if (unlikely(buffer_idx < 0)) {
    printf("No free buffers\n");
    return -1;
  }

  uint32_t *buffer_data = state->buffer_data[buffer_idx];
  struct wl_buffer *buffer = state->buffers[buffer_idx];

  for (int i = 0; i < size; size++) {
    buffer_data[i] = data[i];
  }

  state->buffers_busy[buffer_idx] = true;

  wl_surface_attach(state->surface, buffer, 0, 0);
  wl_surface_damage_buffer(state->surface, 0, 0, WIDTH, HEIGHT);
  wl_surface_commit(state->surface);
  return 0;
}

void fill_transparency_black(struct sd_state *state) {
  if (unlikely(!state->configured)) {
    printf("State not configured!\n");
    return;
  }

  int buffer_idx = get_next_buffer(state);
  if (unlikely(buffer_idx < 0)) {
    printf("No free buffers\n");
    return;
  }

  uint32_t *data = state->buffer_data[buffer_idx];
  struct wl_buffer *buffer = state->buffers[buffer_idx];

  size_t pixel_count = WIDTH * HEIGHT;

  for (size_t i = 0; i < pixel_count; i++) {
    if (state->current_frame[i] == 0) {
      data[i] = 0xFF000000;
    } else {
      data[i] = state->current_frame[i];
    }
  }

  state->buffers_busy[buffer_idx] = true;

  wl_surface_attach(state->surface, buffer, 0, 0);
  wl_surface_damage_buffer(state->surface, 0, 0, WIDTH, HEIGHT);
  wl_surface_commit(state->surface);
}

int clear(struct sd_state *state) {
  if (unlikely(!state->configured)) {
    printf("State not configured!\n");
    return -1;
  }

  int buffer_idx = get_next_buffer(state);
  if (unlikely(buffer_idx < 0)) {
    return buffer_idx;
  }

  uint32_t *data = state->buffer_data[buffer_idx];
  struct wl_buffer *buffer = state->buffers[buffer_idx];

  size_t pixel_count = WIDTH * HEIGHT;

  for (size_t i = 0; i < pixel_count; i++) {
    data[i] = 0;
  }

  state->buffers_busy[buffer_idx] = true;

  wl_surface_attach(state->surface, buffer, 0, 0);
  wl_surface_damage(state->surface, 0, 0, WIDTH, HEIGHT);
  wl_surface_commit(state->surface);

  return 0;
}

int fill_rect(struct sd_state *state, rect rect, color color) {
  int buffer_idx = get_next_buffer(state);
  if (unlikely(buffer_idx < 0)) {
    return buffer_idx;
  }

  uint32_t *data = state->buffer_data[buffer_idx];
  struct wl_buffer *buffer = state->buffers[buffer_idx];

  uint32_t c = (color.a << 24) | (color.r << 16) | (color.g << 8) | color.b;

  for (uint32_t y = 0; y < rect.w; y++) {
    size_t index = (rect.y + y) * WIDTH + rect.x;

    data[index] = c;

    if (rect.l > 1) {
      for (uint32_t x = 1; x < rect.l * 4 / 3; x++) {
        data[index + x] = c;
      }
    }
  }

  state->buffers_busy[buffer_idx] = true;

  wl_surface_attach(state->surface, buffer, 0, 0);
  wl_surface_damage_buffer(state->surface, rect.x, rect.y, rect.l * 4 / 3,
                           rect.w);
  wl_surface_commit(state->surface);

  return 0;
}
