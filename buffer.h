#ifndef BUFFER_H
#define BUFFER_H

#include <pixman.h>
#include <wayland-client.h>

pixman_image_t *create_surface_image(struct wl_shm *shm, int width, int height,
		struct wl_surface *surface);

#endif /* BUFFER_H */
