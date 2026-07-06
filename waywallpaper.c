// TODO: do platform checks for e.g. <sys/...>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <pixman.h>
#include <wayland-client.h>
#include <wlr-layer-shell-unstable-v1.h>

#include "opt.h"
#include "shm.h"

enum display_mode {
	MODE_INVALID = 0,  // default for solid color, otherwise error
	MODE_FILL,
	MODE_FIT,
	MODE_STRETCH,
	MODE_CENTER,
	MODE_TILE,
};

struct state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_list outputs;  // struct output::link
	pixman_color_t color;  // -c option
	enum display_mode display_mode;  // -m option
	bool pixel_perfect;  // -p option
	FILE *image_file;  // -i option
};

struct output {
	struct state *state;
	struct wl_output *wl_output;
	uint32_t wl_name;
	char *name, *description;
	uint32_t width, height;  // buffer size, not actual dimensions
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	bool dirty;  // image needs to be redrawn
	struct wl_list link;
};

static void die(const char *fmt, ...) {
	va_list ap;
	fprintf(stderr, "waywallpaper: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

static void usage(int ret) {
	fprintf(stderr, "usage: waywallpaper [-p] [-c color] [-i image] [-m mode]\n");
	exit(ret);
}

static void noop() {
	// welcome to barrow
}

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial, uint32_t width, uint32_t height) {
	(void)layer_surface;
	(void)serial;

	struct output *output = data;

	if (output->width == width && output->height == height)
		return;

	output->width = width;
	output->height = height;
	output->dirty = true;
	zwlr_layer_surface_v1_ack_configure(output->layer_surface, serial);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	// TODO: kill memory
	.closed = noop,
};

static void output_create_surface(struct output *output) {
	output->surface = wl_compositor_create_surface(output->state->compositor);

	/* passthrough input */
	struct wl_region *input_region = wl_compositor_create_region(output->state->compositor);
	wl_surface_set_input_region(output->surface, input_region);
	wl_region_destroy(input_region);

	output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(output->state->layer_shell, output->surface, output->wl_output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "background");
	zwlr_layer_surface_v1_set_size(output->layer_surface, 0, 0);
	zwlr_layer_surface_v1_set_anchor(output->layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
	zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, -1);
	zwlr_layer_surface_v1_add_listener(output->layer_surface, &layer_surface_listener, output);
	wl_surface_commit(output->surface);
}

static void output_done(void *data, struct wl_output *wl_output) {
	(void)wl_output;

	struct output *output = data;

	fprintf(stderr, "%s", output->name);
	if (output->description)
		fprintf(stderr, " [%s]", output->description);
	fputc('\n', stderr);

	if (!output->surface)
		output_create_surface(output);
}

static void output_name(void *data, struct wl_output *wl_output, const char *name) {
	(void)wl_output;
	struct output *output = data;
	if (output->name)
		free(output->name);
	output->name = strdup(name);
}

static void output_description(void *data, struct wl_output *wl_output, const char *description) {
	(void)wl_output;
	struct output *output = data;
	if (output->description)
		free(output->description);
	char *paren = strchr(description, '(');
	if (paren)
		*--paren = '\0';
	output->description = strdup(description);
}

static const struct wl_output_listener output_listener = {
	.geometry = noop,
	.mode = noop,
	.done = output_done,
	.scale = noop,
	.name = output_name,
	.description = output_description,
};

static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	(void)version;
	struct state *state = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 2);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		// TODO: create output_init()
		struct wl_output *wl_output = wl_registry_bind(registry, name, &wl_output_interface, 4);
		struct output *output = calloc(1, sizeof(*output));
		output->state = state;
		output->wl_output = wl_output;
		output->wl_name = name;
		wl_output_add_listener(output->wl_output, &output_listener, output);
		wl_list_insert(&state->outputs, &output->link);
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	// TODO: handle removing outputs
	.global_remove = noop,
};

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	(void)data;
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener buffer_listener = {
	.release = wl_buffer_release,
};

void unmap_image_data(pixman_image_t *image, void *data) {
	(void)data;
	munmap(pixman_image_get_data(image), pixman_image_get_height(image) * pixman_image_get_stride(image));
}

static pixman_image_t *create_surface_image(struct wl_shm *shm, struct wl_surface *surface, uint32_t width, uint32_t height) {
	const uint32_t stride = width * 4;
	const uint32_t size = height * stride;

	// TODO: error checks (fd != -1; data != MAP_FAILED; image != NULL)
	int fd = allocate_shm_file(size);
	uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	wl_buffer_add_listener(buffer, &buffer_listener, NULL);
	wl_surface_attach(surface, buffer, 0, 0);
	pixman_image_t *image = pixman_image_create_bits_no_clear(PIXMAN_x8r8g8b8, width, height, data, stride);
	pixman_image_set_destroy_function(image, unmap_image_data, NULL);
	return image;
}

int main(int argc, char **argv) {
	struct state state = {0};

	OPTBEGIN(argc, argv) {
		case 'p':
			state.pixel_perfect = true;
			break;
		case 'c': {
			const char *color = OPTARG;
			uint32_t r = 0, g = 0, b = 0;
			if (sscanf(color, "%02x%02x%02x", &r, &g, &b) != 3)
				die("failed to parse color: %s", color);
			state.color.red = (double)r * 0xffff / 0xff;
			state.color.green = (double)g * 0xffff / 0xff;
			state.color.blue = (double)b * 0xffff / 0xff;
		} break;
		case 'i': {
			const char *image_path = OPTARG;
			state.image_file = fopen(image_path, "rb");
			if (!state.image_file)
				die("failed to open image: %s", image_path);
		} break;
		case 'm': {
			const char *mode = OPTARG;
			if (strcmp(mode, "fill") == 0)
				state.display_mode = MODE_FILL;
			else if (strcmp(mode, "fit") == 0)
				state.display_mode = MODE_FIT;
			else if (strcmp(mode, "stretch") == 0)
				state.display_mode = MODE_STRETCH;
			else if (strcmp(mode, "center") == 0)
				state.display_mode = MODE_CENTER;
			else if (strcmp(mode, "tile") == 0)
				state.display_mode = MODE_TILE;
			else
				die("failed to parse mode: %s", mode);
		} break;
	} OPTEND;

	wl_list_init(&state.outputs);

	if (!(state.display = wl_display_connect(NULL)))
		die("cannot connect to display");
	state.registry = wl_display_get_registry(state.display);
	wl_registry_add_listener(state.registry, &registry_listener, &state);
	wl_display_roundtrip(state.display);
	if (!(state.compositor && state.shm && state.layer_shell))
		die("unsupported compositor");

	while (wl_display_dispatch(state.display) != -1) {
		wl_display_flush(state.display);
		struct output *output;
		wl_list_for_each(output, &state.outputs, link) {
			if (output->dirty) {
				output->dirty = false;
				pixman_image_t *image = create_surface_image(output->state->shm, output->surface, output->width, output->height);
				pixman_image_fill_rectangles(PIXMAN_OP_SRC, image, &state.color, 1, &(pixman_rectangle16_t){0, 0, output->width, output->height});
				wl_surface_commit(output->surface);
				pixman_image_unref(image);
			}
		}
	}

	// TODO: remove outputs

	if (state.image_file)
		fclose(state.image_file);
	if (state.layer_shell)
		zwlr_layer_shell_v1_destroy(state.layer_shell);
	if (state.shm)
		wl_shm_destroy(state.shm);
	if (state.compositor)
		wl_compositor_destroy(state.compositor);
	if (state.registry)
		wl_registry_destroy(state.registry);
	if (state.display)
		wl_display_disconnect(state.display);

	return EXIT_SUCCESS;
}
