#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <pixman.h>
#include <wayland-client.h>
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "tllist/tllist.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

#include "buffer.h"

#define WARN(fmt, ...) fprintf(stderr, ("jab: " fmt "\n"), ##__VA_ARGS__)

enum { ModeFill, ModeFit, ModeStretch, ModeCenter, ModeTile, ModeInvalid }; /* image mode */

struct jab_image {
	int width, height, channels;
	unsigned char *data;
	pixman_image_t *image;
	pixman_format_code_t format;
};

struct jab_config {
	pixman_image_t *image;
	pixman_color_t color;
	int mode;
	bool pixel_perfect;
	char *identifier;
	char *image_path;

	/* internal */
	pixman_format_code_t format;
};

struct jab_output {
	struct wl_output *wl_output;
	uint32_t wl_name;

	char *name, *identifier;
	int scale, width, height;

	struct jab_config *config;

	bool configured;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
};

/* application state */
static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;
static bool running;
static tll(struct jab_config *) configs;
static tll(struct jab_output *) outputs;

/* main config all outputs inherit from */
static struct jab_config main_config = {
	.image = NULL,
	.color = (pixman_color_t){0, 0, 0, 65535},
	.mode = ModeInvalid,
	.pixel_perfect = false,
	.identifier = "",
};

static const char usage[] = "usage: jab [-hVn] [-c color] [-i image] [-m mode] [-o output]\n";

static void
noop()
{
	/* this space intentionally left blank */
}

static struct jab_config *
create_config(void)
{
	struct jab_config *config = malloc(sizeof(struct jab_config));
	if (!config)
		return NULL;
	memcpy(config, &main_config, sizeof(struct jab_config));
	return config;
}

static bool
parse_color(const char *color, pixman_color_t *result)
{
	int count, r, g, b;
	count = sscanf(color, "%02x%02x%02x", &r, &g, &b);
	if (count != 3)
		return false;
	result->red = (double)r * 0xffff / 0xff;
	result->green = (double)g * 0xffff / 0xff;
	result->blue = (double)b * 0xffff / 0xff;
	return true;
}

static int
parse_mode(const char *mode)
{
	if (!strcmp(mode, "fill"))
		return ModeFill;
	else if (!strcmp(mode, "fit"))
		return ModeFit;
	else if (!strcmp(mode, "stretch"))
		return ModeStretch;
	else if (!strcmp(mode, "center"))
		return ModeCenter;
	else if (!strcmp(mode, "tile"))
		return ModeTile;
	return ModeInvalid;
}

static void
render(struct jab_output *output)
{
	struct jab_config *config = output->config;
	int image_width, image_height;
	struct jab_buffer *buffer = create_buffer(shm, output->width, output->height);
	double sx, sy, s;
	pixman_transform_t t;

	/* draw color */
	pixman_image_fill_rectangles(PIXMAN_OP_SRC, buffer->image, &config->color, 1,
			&(pixman_rectangle16_t){0, 0, output->width, output->height});

	/* draw image if specified */
	if (config->image) {
		image_width = pixman_image_get_width(config->image);
		image_height = pixman_image_get_height(config->image);
		if (config->mode == ModeFill || config->mode == ModeFit) {
			sx = (double)(output->width) / image_width;
			sy = (double)(output->height) / image_height;
			s = config->mode == ModeFill ? fmax(sx, sy) : fmin(sx, sy);

			pixman_transform_init_scale(&t, pixman_double_to_fixed(1 / s),
					pixman_double_to_fixed(1 / s));
			pixman_transform_translate(&t, NULL,
					pixman_int_to_fixed((image_width - output->width / s) / 2),
					pixman_int_to_fixed((image_height - output->height / s) / 2));
			pixman_image_set_transform(config->image, &t);
			if (!config->pixel_perfect)
				pixman_image_set_filter(config->image, PIXMAN_FILTER_BEST, NULL, 0);
			pixman_image_composite32(PIXMAN_OP_OVER, config->image, NULL, buffer->image, 0,
					0, 0, 0, 0, 0, output->width, output->height);
		} else if (config->mode == ModeStretch) {
			pixman_transform_init_scale(&t,
					pixman_double_to_fixed(1 / ((double)(output->width) / image_width)),
					pixman_double_to_fixed(1 / ((double)(output->height) / image_height)));
			pixman_image_set_transform(config->image, &t);
			if (!config->pixel_perfect)
				pixman_image_set_filter(config->image, PIXMAN_FILTER_BEST, NULL, 0);
			pixman_image_composite32(PIXMAN_OP_OVER, config->image, NULL, buffer->image, 0,
					0, 0, 0, 0, 0, output->width, output->height);
		} else if (config->mode == ModeCenter) {
			pixman_transform_init_translate(&t,
					pixman_int_to_fixed((image_width - output->width) / 2),
					pixman_int_to_fixed((image_height - output->height) / 2));
			pixman_image_set_transform(config->image, &t);
			pixman_image_composite32(PIXMAN_OP_OVER, config->image, NULL, buffer->image, 0,
					0, 0, 0, 0, 0, output->width, output->height);
		} else if (config->mode == ModeTile)
			for (int y = 0; y < output->height; y += image_height)
				for (int x = 0; x < output->width; x += image_width)
					pixman_image_composite32(PIXMAN_OP_OVER, config->image, NULL, buffer->image, 0,
							0, 0, 0, x, y, image_width, image_height);
	}

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
destroy_layer_output(struct jab_output *output)
{
	if (output->layer_surface)
		zwlr_layer_surface_v1_destroy(output->layer_surface);
	if (output->surface)
		wl_surface_destroy(output->surface);
	output->configured = false;
}

static void
destroy_config(struct jab_config *config)
{
	if (config->image) {
		pixman_image_unref(config->image);
		stbi_image_free(pixman_image_get_data(config->image));
	}
	free(config->image_path);
	free(config->identifier);
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
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
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
output_done(void *data, struct wl_output *wl_output)
{
	struct jab_output *output = data;
	struct jab_config *config;
	fprintf(stderr, "output: %s (%s)\n", output->name, output->identifier);
	output->config = &main_config;
	tll_foreach(configs, it) {
		config = it->item;
		if (!strcmp(output->name, config->identifier) ||
				!strcmp(output->identifier, config->identifier)) {
			output->config = config;
			break;
		}
	}
}

static void
output_scale(void *data, struct wl_output *wl_output, int factor)
{
	struct jab_output *output = data;
	output->scale = factor;
	if (output->configured)
		render(output);
}

static void
output_name(void *data, struct wl_output *wl_output, const char *name)
{
	struct jab_output *output = data;
	free(output->name);
	output->name = strdup(name);
}

static void
output_description(void *data, struct wl_output *wl_output, const char *description)
{
	struct jab_output *output = data;
	char buf[256], *identifier;
	strcpy(buf, description);
	identifier = strtok(buf, "(");
	identifier[strlen(identifier) - 1] = '\0';
	free(output->identifier);
	output->identifier = strdup(identifier);
}

static const struct wl_output_listener output_listener = {
	.geometry = noop,
	.mode = noop,
	.done = output_done,
	.scale = output_scale,
	.name = output_name,
	.description = output_description,
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
		output->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 4);
		tll_push_back(outputs, output);
		wl_output_add_listener(output->wl_output, &output_listener, output);
		add_surface_to_output(output);
	}
}

static void
destroy_output(struct jab_output *output)
{
	destroy_layer_output(output);
	wl_output_release(output->wl_output);
	free(output->name);
	free(output->identifier);
	free(output);
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	tll_foreach(outputs, it)
		if (it->item->wl_name == name) {
			destroy_output(it->item);
			break;
		}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
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
	int ret, c;
	struct jab_config *config = &main_config;
	pixman_color_t color;
	opterr = 0;

	while ((c = getopt(argc, argv, "hVpc:i:m:o:")) != -1)
		switch (c) {
			case 'h':
				fputs(usage, stderr);
				exit(EXIT_SUCCESS);
			case 'V':
				fputs("jab v"VERSION"\n", stderr);
				exit(EXIT_SUCCESS);
			case 'p':
				config->pixel_perfect = true;
				break;
			case 'c':
				if (!parse_color(optarg, &color))
					exit(EXIT_FAILURE);
				config->color = color;
				break;
			case 'i':
				config->image_path = strdup(optarg);
				break;
			case 'm':
				config->mode = parse_mode(optarg);
				if (config->mode == ModeInvalid)
					exit(EXIT_FAILURE);
				break;
			case 'o':
				config = create_config();
				if (!config)
					exit(EXIT_FAILURE);
				config->identifier = strdup(optarg);
				tll_push_back(configs, config);
				break;
			case '?':
				if (optopt == 'c' || optopt == 'i' || optopt == 'm' || optopt == 'o')
					WARN("option requires argument: -%c", optopt);
				else
					WARN("unknown option: -%c", optopt);
				fputs(usage, stderr);
				/* fallthrough */
			default:
				exit(EXIT_FAILURE);
		}

	tll_push_back(configs, &main_config);
	tll_foreach(configs, it) {
		struct jab_config *config = it->item;
		if (config->image_path) {
			pixman_format_code_t format;
			int width, height, channels;
			const unsigned char *buf = stbi_load(config->image_path, &width, &height, &channels, 4);

			if (buf) {
				format = channels == 4 ? PIXMAN_a8b8g8r8 : PIXMAN_b8g8r8;
				format = PIXMAN_a8b8g8r8;
				config->image = pixman_image_create_bits_no_clear(format, width, height,
						(uint32_t *)buf, stride_for_format_and_width(format, width));
			} else {
				fprintf(stderr, "failed to read image\n");
			}
		}
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

	ret = EXIT_SUCCESS;
	running = true;
	while (running && wl_display_dispatch(display) != -1) {
	}

finish:
	tll_free_and_free(configs, destroy_config);
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
