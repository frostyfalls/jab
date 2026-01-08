#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <pixman.h>
#include <wayland-client.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "tllist/tllist.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "buffer.h"

#define WARN(fmt, ...) fprintf(stderr, ("jab: " fmt "\n"), ##__VA_ARGS__)

enum { ModeFill, ModeFit, ModeStretch, ModeCenter, ModeTile, ModeInvalid }; /* image mode */

struct jab_image {
	int width;
	int height;
	int channels;
	pixman_format_code_t format;
	pixman_image_t *pix;
};

struct jab_output {
	struct wl_output *wl_output;
	uint32_t wl_name;
	char *make, *model;
	int scale, width, height;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	bool configured;
};

/* application state */
static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;
static tll(struct jab_output *) outputs;
static bool running;
static struct jab_image image = {0};

/* config */
static uint32_t color = 0x000000FF;
static char *image_path = NULL;
static int image_mode = ModeInvalid;
static bool nearest_neighbor = false;

static const char usage[] = "usage: jab [-hVn] [-c COLOR] [-i IMAGE_PATH] [-m MODE] [-o OUTPUT]\n";

static void
noop()
{
	/* this space intentionally left blank */
}

/* thank you sewn for drwl */
static inline pixman_color_t
convert_color(const uint32_t color)
{
	return (pixman_color_t){
		((color >> 24) & 0xFF) * 0x101 * (color & 0xFF) / 0xFF,
		((color >> 16) & 0xFF) * 0x101 * (color & 0xFF) / 0xFF,
		((color >> 8) & 0xFF) * 0x101 * (color & 0xFF) / 0xFF,
		(color & 0xFF) * 0x101
	};
}

static bool
parse_color(const char *color, uint32_t *result)
{
	int len, i;

	if (color[0] == '#')
		color++;

	len = strlen(color);
	if (len != 6)
		return false;
	for (i = 0; i < len; i++)
		if (!isxdigit(color[i]))
			return false;

	uint32_t val = strtoul(color, NULL, 16);
	*result = (val << 8) | 0xFF;
	return true;
}

static bool
parse_mode(const char *mode, int *result)
{
	if (!strcmp(mode, "fill"))
		*result = ModeFill;
	else if (!strcmp(mode, "fit"))
		*result = ModeFit;
	else if (!strcmp(mode, "stretch"))
		*result = ModeStretch;
	else if (!strcmp(mode, "center"))
		*result = ModeCenter;
	else if (!strcmp(mode, "tile"))
		*result = ModeTile;
	else
		return false;
	return true;
}

static void
destroy_layer_output(struct jab_output *output)
{
	if (output->layer_surface)
		zwlr_layer_surface_v1_destroy(output->layer_surface);
	if (output->surface)
		wl_surface_destroy(output->surface);
	output->configured = false;
}

static void
destroy_output(struct jab_output *output)
{
	destroy_layer_output(output);
	wl_output_release(output->wl_output);
	free(output->make);
	free(output->model);
	free(output);
}

static void
render(struct jab_output *output)
{
	struct jab_buffer *buffer;
	pixman_color_t pix_color = convert_color(color);
	double sx, sy, s;
	pixman_transform_t t;

	buffer = create_buffer(shm, output->width, output->height);
	buffer->image = pixman_image_create_bits_no_clear(PIXMAN_x8r8g8b8, output->width,
			output->height, buffer->data, output->width * 4);

	/* draw selected bg color */
	pixman_image_fill_rectangles(PIXMAN_OP_SRC, buffer->image, &pix_color, 1,
			&(pixman_rectangle16_t){0, 0, output->width, output->height});

	/* draw image if specified */
	if (image_mode == ModeFill || image_mode == ModeFit) {
		sx = (double)(output->width) / image.width;
		sy = (double)(output->height) / image.height;
		s = image_mode == ModeFill ? fmax(sx, sy) : fmin(sx, sy);

		pixman_transform_init_scale(&t, pixman_double_to_fixed(1 / s),
				pixman_double_to_fixed(1 / s));
		pixman_transform_translate(&t, NULL,
				pixman_int_to_fixed((image.width - output->width / s) / 2),
				pixman_int_to_fixed((image.height - output->height / s) / 2));
		pixman_image_set_transform(image.pix, &t);
		if (!nearest_neighbor)
			pixman_image_set_filter(image.pix, PIXMAN_FILTER_BEST, NULL, 0);

		pixman_image_composite32(PIXMAN_OP_OVER, image.pix, NULL, buffer->image, 0, 0, 0, 0, 0, 0,
				output->width, output->height);
	} else if (image_mode == ModeStretch) {
		pixman_transform_init_scale(&t,
				pixman_double_to_fixed(1 / ((double)(output->width) / image.width)),
				pixman_double_to_fixed(1 / ((double)(output->height) / image.height)));
		pixman_image_set_transform(image.pix, &t);
		if (!nearest_neighbor)
			pixman_image_set_filter(image.pix, PIXMAN_FILTER_BEST, NULL, 0);

		pixman_image_composite32(PIXMAN_OP_OVER, image.pix, NULL, buffer->image, 0, 0, 0, 0, 0, 0,
				output->width, output->height);
	} else if (image_mode == ModeCenter) {
		pixman_transform_init_translate(&t, pixman_int_to_fixed(image.width - output->width / 2),
				pixman_int_to_fixed(image.height - output->height / 2));
		pixman_image_set_transform(image.pix, &t);

		pixman_image_composite32(PIXMAN_OP_OVER, image.pix, NULL, buffer->image, 0, 0, 0, 0, 0, 0,
				output->width, output->height);
	} else if (image_mode == ModeTile)
		for (int y = 0; y < output->height; y += image.height)
			for (int x = 0; x < output->width; x += image.width)
				pixman_image_composite32(PIXMAN_OP_SRC, image.pix, NULL, buffer->image, 0, 0, 0, 0,
						x, y, image.width, image.height);

	wl_surface_set_buffer_scale(output->surface, output->scale);
	wl_surface_attach(output->surface, buffer->wl_buffer, 0, 0);
	wl_surface_damage(output->surface, 0, 0, UINT32_MAX, UINT32_MAX);
	wl_surface_commit(output->surface);

	destroy_buffer(buffer);
}

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
		uint32_t width, uint32_t height)
{
	struct jab_output *output = data;

	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

	if (output->configured && output->width == width && output->height == height) {
		wl_surface_commit(output->surface);
		return;
	}

	output->width = width;
	output->height = height;
	output->configured = true;
	render(output);
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
	struct jab_output *output = data;

	tll_foreach(outputs, it)
		if (it->item == output) {
			destroy_layer_output(output);
			break;
		}
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = &layer_surface_configure,
    .closed = &layer_surface_closed,
};

static void
add_surface_to_output(struct jab_output *output)
{
	struct wl_surface *surface;
	struct wl_region *region;
	struct zwlr_layer_surface_v1 *layer_surface;

	surface = wl_compositor_create_surface(compositor);

	region = wl_compositor_create_region(compositor);
	wl_surface_set_opaque_region(surface, region);
	wl_region_destroy(region);

	layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, surface, output->wl_output,
			ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "background");
	zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);
	zwlr_layer_surface_v1_set_anchor(layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);

	output->surface = surface;
	output->layer_surface = layer_surface;

	zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, output);
	wl_surface_commit(surface);
}

static void
output_geometry(void *data, struct wl_output *wl_output, int x, int y, int physical_width,
		int physical_height, int subpixel, const char *make, const char *model, int transform)
{
	struct jab_output *output = data;
	free(output->make);
	free(output->model);
	output->make = make != NULL ? strdup(make) : NULL;
	output->model = model != NULL ? strdup(model) : NULL;
}

static void
output_mode(void *data, struct wl_output *wl_output, uint32_t flags, int width, int height,
		int refresh)
{
	struct jab_output *output = data;
	output->width = width;
	output->height = height;
}

static void
output_done(void *data, struct wl_output *wl_output)
{
	struct jab_output *output = data;
	fprintf(stderr, "output: %s %s (%dx%d, scale=%d)\n", output->make, output->model,
			output->width, output->height, output->scale);
}

static void
output_scale(void *data, struct wl_output *wl_output, int factor)
{
	struct jab_output *output = data;
	output->scale = factor;
	if (output->configured)
		render(output);
}

static const struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
		uint32_t version)
{
	struct jab_output *output;

	if (strcmp(interface, wl_compositor_interface.name) == 0)
		compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);

	else if (strcmp(interface, wl_shm_interface.name) == 0)
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);

	else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
		layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 2);

	else if (strcmp(interface, wl_output_interface.name) == 0) {
		output = calloc(1, sizeof(struct jab_output));
		output->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 3);
		tll_push_back(outputs, output);
		wl_output_add_listener(output->wl_output, &output_listener, output);
		add_surface_to_output(output);
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = noop,
};

/* TODO: optimize or figure out why this works? */
/* thanks to dnkl for wbg */
static inline int
stride_for_format_and_width(pixman_format_code_t format, const int width)
{
    return (((PIXMAN_FORMAT_BPP(format) * width + 7) / 8 + 4 - 1) & -4);
}

int
main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	char c;
	unsigned char *stb_image = NULL;

	opterr = 0;
	while ((c = getopt(argc, argv, "hVnc:i:m:o:")) != -1)
		switch (c) {
		case 'h':
			fputs(usage, stderr);
			return EXIT_SUCCESS;
		case 'V':
			fputs("jab v"VERSION"\n", stderr);
			return EXIT_SUCCESS;
		case 'n':
			nearest_neighbor = true;
			break;
		case 'c':
			if (!parse_color(optarg, &color)) {
				WARN("invalid color: %s", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'i':
			/* TODO: do more checks for a valid file? */
			image_path = optarg;
			break;
		case 'm':
			if (!parse_mode(optarg, &image_mode)) {
				WARN("invalid mode: %s", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'o':
			WARN("TODO");
			return EXIT_FAILURE;
		case '?':
			if (optopt == 'c' || optopt == 'i' || optopt == 'm' || optopt == 'o')
				WARN("option requires argument: -%c", optopt);
			else
				WARN("unknown option: -%c", optopt);
			fputs(usage, stderr);
			/* fallthrough */
		default:
			return EXIT_FAILURE;
		}

	if (image_path && image_mode == ModeInvalid) {
		WARN("no mode specified");
		goto finish;
	}

	if (image_path) {
		stb_image = stbi_load(image_path, &image.width, &image.height, &image.channels, 0);
		if (!stb_image) {
			WARN("failed to read image");
			goto finish;
		}
		image.format = image.channels == 4 ? PIXMAN_a8b8g8r8 : PIXMAN_b8g8r8;
		image.pix = pixman_image_create_bits_no_clear(image.format, image.width, image.height,
				(uint32_t *)stb_image, stride_for_format_and_width(image.format, image.width));
	}

	display = wl_display_connect(NULL);
	if (!display) {
		WARN("failed to connect to display");
		goto finish;
	}

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (!compositor || !shm || !layer_shell) {
		WARN("unsupported compositor");
		goto finish;
	}
	if (!tll_length(outputs)) {
		WARN("no outputs");
		goto finish;
	}

	ret = EXIT_SUCCESS;
	running = true;
	while (running && wl_display_dispatch(display) != -1);

finish:
	if (image.pix)
		pixman_image_unref(image.pix);
	if (stb_image)
		stbi_image_free(stb_image);
	tll_free_and_free(outputs, destroy_output);
	if (layer_shell)
		zwlr_layer_shell_v1_destroy(layer_shell);
	if (shm)
		wl_shm_destroy(shm);
	if (compositor)
		wl_compositor_destroy(compositor);
	if (registry)
		wl_registry_destroy(registry);
	if (display)
		wl_display_disconnect(display);

	return ret;
}
