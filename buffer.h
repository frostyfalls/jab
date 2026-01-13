#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>

#include <pixman.h>
#include <wayland-client.h>

struct jab_buffer {
	struct wl_buffer *wl_buffer;
	pixman_image_t *image;
	uint32_t *data;
	size_t size;
};

struct jab_buffer *create_buffer(struct wl_shm *shm, int width, int height);
void init_buffer(struct wl_shm *shm, int width, int height, struct wl_buffer *buffer, pixman_image_t *image);
void destroy_buffer(struct jab_buffer *buffer);

#endif /* POOL_BUFFER_H */
