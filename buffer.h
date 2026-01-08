#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>

#include <pixman.h>
#include <wayland-client.h>

struct jab_buffer {
	struct wl_buffer *wl_buffer;
	uint32_t *data;
	size_t size;
	int stride;
	pixman_image_t *image;
};

struct jab_buffer *create_buffer(struct wl_shm *shm, int width, int height);
void destroy_buffer(struct jab_buffer *buffer);

#endif /* BUFFER_H */
