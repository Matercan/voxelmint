#ifndef __WL_CLIENT_H
#define __WL_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <wayland-client-protocol.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define WIDTH 1920
#define HEIGHT 1080

#pragma once

enum pointer_event_mask {
  POINTER_EVENT_ENTER = 1 << 0,
  POINTER_EVENT_LEAVE = 1 << 1,
  POINTER_EVENT_MOTION = 1 << 2,
  POINTER_EVENT_BUTTON = 1 << 3,
  POINTER_EVENT_AXIS = 1 << 4,
  POINTER_EVENT_AXIS_SOURCE = 1 << 5,
  POINTER_EVENT_AXIS_STOP = 1 << 6,
  POINTER_EVENT_AXIS_DISCRETE = 1 << 7,
};

struct pointer_event {
  uint32_t event_mask;
  wl_fixed_t surface_x, surface_y;
  uint32_t button, state;
  uint32_t time;
  uint32_t serial;
  struct {
    bool valid;
    wl_fixed_t value;
    int32_t discrette;
  } axes[2];
  uint32_t axis_source;
};

struct sd_state {
  bool configured;
  struct pointer_event pointer_event;

  /* Buffer information */
  struct wl_buffer *buffers[2];
  uint32_t *buffer_data[2];
  int current_buffer;
  bool buffers_busy[2];
  uint32_t *current_frame;

  /* Wayland global information */
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_shm *shm;
  struct wl_compositor *compositor;
  struct xdg_wm_base *wm_base;
  struct wl_seat *seat;

  /* Surface information */
  struct wl_surface *surface;
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct wl_keyboard *keyboard;
  struct wl_pointer *pointer;
};

extern struct sd_state *wl_state;

int8_t create_display();
int allocate_shm_file(size_t);
int get_next_buffer(struct sd_state *);
int dispatch_display(struct sd_state *);

#endif // !__WL_CLIENT_H
