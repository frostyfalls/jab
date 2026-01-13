#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <pixman.h>
#include <wayland-client.h>

#include "buffer.h"

/* TODO: cleanup file */

static void
randname(char *buf)
{
	struct timespec ts;
	long r;
	int i;

	clock_gettime(CLOCK_REALTIME, &ts);
	r = ts.tv_nsec;
	for (i = 0; i < 6; i++) {
		buf[i] = 'A' + (r & 15) + (r & 16) * 2;
		r >>= 5;
	}
}

static int
create_shm_file(void)
{
	char name[] = "/jab-XXXXXX";
	int retries = 100, fd;

	do {
		randname(name + sizeof(name) - 7);
		retries--;
		fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);

	return -1;
}

static int
allocate_shm_file(size_t size)
{
	int fd = create_shm_file(), ret;

	if (fd < 0)
		return -1;
	do
		ret = ftruncate(fd, size);
	while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static void
wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener buffer_listener = {
    .release = wl_buffer_release,
};

struct jab_buffer *
create_buffer(struct wl_shm *shm, int width, int height)
{
	const int index = 0;
	const int stride = width * 4;
	const int size = height * stride * 2;
	const int offset = height * stride * index;

	int fd = allocate_shm_file(size);
	if (fd == -1)
		return NULL;

	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *wl_buffer = wl_shm_pool_create_buffer(pool, offset, width, height, stride,
			WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	struct jab_buffer *buffer = calloc(1, sizeof(struct jab_buffer));

	pixman_image_t *image = pixman_image_create_bits_no_clear(PIXMAN_x8r8g8b8, width,
			height, data, stride);

	buffer->wl_buffer = wl_buffer;
	buffer->size = size;
	buffer->data = data;
	buffer->image = image;

	wl_buffer_add_listener(buffer->wl_buffer, &buffer_listener, buffer);

	return buffer;
}

void
destroy_buffer(struct jab_buffer *buffer)
{
	pixman_image_unref(buffer->image);
	wl_buffer_destroy(buffer->wl_buffer);
	munmap(buffer->data, buffer->size);
}
