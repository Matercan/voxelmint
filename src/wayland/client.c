#include "client.h"
#include "protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

struct sd_state *wl_state = 0;

static void randname(char *buf) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  long r = ts.tv_nsec;
  for (int i = 0; i < 6; i++) {
    buf[i] = (char)('A' + (r & 15) + (r & 16) * 2);
    r >>= 5;
  }
}

static int create_shm_file(void) {
  int retries = 100;
  do {
    char name[] = "/wl_shm-XXXXXX";
    randname(name + sizeof(name) - 7);
    --retries;
    int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd >= 0) {
      shm_unlink(name);
      return fd;
    }
  } while (retries > 0 && errno == EEXIST);
  return -1;
}

int allocate_shm_file(size_t size) {
  int fd = create_shm_file();
  if (fd < 0)
    return -1;
  int ret;
  do {
    ret = ftruncate(fd, (long int)size);
  } while (ret < 0 && errno == EINTR);
  if (ret < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static void wl_buffer_release(void *data, struct wl_buffer *buffer) {
  int *buffer_index = (int *)data;
  wl_state->buffers_busy[*buffer_index] = false;
  for (size_t i = 0; i < WIDTH * HEIGHT; i++) {
    if (wl_state->buffer_data[*buffer_index][i] != 0) {
      wl_state->current_frame[i] = wl_state->buffer_data[*buffer_index][i];
    }
  }
}

static void wl_surface_enter(void *data, struct wl_surface *wl_surface,
                             struct wl_output *output) {}

static void wl_surface_leave(void *data, struct wl_surface *wl_surface,
                             struct wl_output *output) {
  free(wl_state->current_frame);
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
                             uint32_t serial, struct wl_surface *surface,
                             wl_fixed_t surface_x, wl_fixed_t surface_y) {
  struct sd_state *client_state = data;
  client_state->pointer_event.event_mask |= POINTER_EVENT_ENTER;
  client_state->pointer_event.serial = serial;
  client_state->pointer_event.surface_x = surface_x,
  client_state->pointer_event.surface_y = surface_y;
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
                             uint32_t serial, struct wl_surface *surface) {
  struct sd_state *client_state = data;
  client_state->pointer_event.serial = serial;
  client_state->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
}

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer,
                              uint32_t time, wl_fixed_t surface_x,
                              wl_fixed_t surface_y) {
  struct sd_state *client_state = data;
  client_state->pointer_event.event_mask |= POINTER_EVENT_MOTION;
  client_state->pointer_event.time = time;
  client_state->pointer_event.surface_x = surface_x,
  client_state->pointer_event.surface_y = surface_y;
}

extern void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer);

static const struct wl_pointer_listener wl_pointer_listener = {
    .enter = wl_pointer_enter,
    .leave = wl_pointer_leave,
    .motion = wl_pointer_motion,
    .frame = wl_pointer_frame,
};

static void wl_seat_capabilies(void *data, struct wl_seat *wl_seaet,
                               uint32_t capabilities) {
  struct sd_state *state = data;

  bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;

  if (have_pointer && state->pointer == NULL) {
    state->pointer = wl_seat_get_pointer(state->seat);
    wl_pointer_add_listener(state->pointer, &wl_pointer_listener, state);
  } else if (!have_pointer && state->pointer != NULL) {
    wl_pointer_release(state->pointer);
    state->pointer = NULL;
  }
}

static void wl_seat_name(void *data, struct wl_seat *wl_seat,
                         const char *name) {}

static const struct wl_seat_listener wl_seat_listener = {
    .capabilities = wl_seat_capabilies,
    .name = wl_seat_name,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface,
                                   uint32_t version) {
  struct sd_state *state = data;
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    state->compositor =
        wl_registry_bind(registry, name, &wl_compositor_interface, 4);
  }
  if (strcmp(interface, wl_shm_interface.name) == 0) {
    state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
  }
  if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    state->wm_base =
        wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(state->wm_base, &xdg_wm_base_listener, state);
  }
  if (strcmp(interface, wl_seat_interface.name) == 0) {
    state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
    wl_seat_add_listener(state->seat, &wl_seat_listener, state);
  }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t name) {};

int wl_display_init_shm(struct wl_display *display);

uint32_t *wl_display_add_shm_format(struct wl_display *display,
                                    uint32_t format);

const struct wl_surface_listener surface_listener = {
    .enter = wl_surface_enter,
    .leave = wl_surface_leave,
};

const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

const struct wl_buffer_listener buffer_listener = {
    .release = wl_buffer_release,
};

void init_buffers(struct sd_state *state) {
  const int stride = WIDTH * 4;
  const int size = HEIGHT * stride;

  for (int i = 0; i < 2; i++) {
    int fd = allocate_shm_file(size);
    if (fd == -1)
      return;

    state->buffer_data[i] =
        mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (state->buffer_data[i] == MAP_FAILED) {
      close(fd);
      return;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
    state->buffers[i] = wl_shm_pool_create_buffer(
        pool, 0, WIDTH, HEIGHT, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    state->buffers_busy[i] = false;

    static int indicies[2] = {0, 1};
    wl_buffer_add_listener(state->buffers[i], &buffer_listener, &indicies[i]);
  }

  state->current_buffer = 0;
}

int get_next_buffer(struct sd_state *state) {
  int next = (state->current_buffer + 1) % 2;
  if (!state->buffers_busy[next]) {
    state->current_buffer = next;
    return next;
  }

  if (!state->buffers_busy[state->current_buffer]) {
    return state->current_buffer;
  }

  return -1;
}

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                  uint32_t serial) {
  struct sd_state *state = data;
  xdg_surface_ack_configure(xdg_surface, serial);
  state->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

int dispatch_display(struct sd_state *state) {
    return wl_display_dispatch(state->display); 
}

int8_t create_display() {
  struct sd_state *state = malloc(sizeof(struct sd_state));
  state->configured = false;
  state->display = wl_display_connect(0);
  state->registry = wl_display_get_registry(state->display);
  wl_registry_add_listener(state->registry, &registry_listener, state);

  wl_display_roundtrip(state->display);
  state->current_frame = malloc(sizeof(uint32_t) * WIDTH * HEIGHT);

  state->surface = wl_compositor_create_surface(state->compositor);
  state->xdg_surface =
      xdg_wm_base_get_xdg_surface(state->wm_base, state->surface);
  xdg_surface_add_listener(state->xdg_surface, &xdg_surface_listener, state);
  state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
  xdg_toplevel_set_title(state->xdg_toplevel, "SUDOKU");

  wl_surface_commit(state->surface);

  init_buffers(state);
  wl_state = state;

  return 0;
}
