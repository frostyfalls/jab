#include <fcntl.h>
#include <pixman.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

static int
allocate_shm_file(size_t size)
{
	char name[] = "/jab-shm-buffer";
	int fd;

	fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd == -1 || ftruncate(fd, size) == -1)
		return -1;

	shm_unlink(name);
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

void
unmap_image_data(pixman_image_t *image, void *data)
{
	munmap(pixman_image_get_data(image), pixman_image_get_height(image) * pixman_image_get_stride(image));
}

pixman_image_t *
create_surface_image(struct wl_shm *shm, int width, int height, struct wl_surface *surface)
{
	const int stride = width * 4;
	const int size = height * stride;
	int fd;
	uint32_t *data;
	struct wl_shm_pool *pool;
	struct wl_buffer *wl_buffer;
	pixman_image_t *image;

	fd = allocate_shm_file(size);
	if (fd == -1)
		goto err;

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED)
		goto err;

	pool = wl_shm_create_pool(shm, fd, size);
	wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	wl_buffer_add_listener(wl_buffer, &buffer_listener, NULL);
	wl_surface_attach(surface, wl_buffer, 0, 0);
	image = pixman_image_create_bits_no_clear(PIXMAN_x8r8g8b8, width, height, data, stride);
	pixman_image_set_destroy_function(image, unmap_image_data, NULL);
	return image;

err:
	if (fd >= 0)
		close(fd);
	return NULL;
}
