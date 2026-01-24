#include <getopt.h>
#include <pixman.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>
#include "tllist/tllist.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "buffer.h"
#include "image-mode.h"

/* Image display mode */
enum { ModeFill, ModeFit, ModeStretch, ModeCenter, ModeTile, ModeInvalid };

struct jab_image {
	unsigned char *buf;
	int width, height;
};

struct jab_output {
	struct wl_output *wl_output;
	uint32_t wl_name;

	char name[256], identifier[256];
	uint32_t width, height;

	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	bool dirty, needs_ack;
	uint32_t configure_serial;
};

/* Configuration */
static bool pixel_perfect = false;
static pixman_color_t color = {0, 0, 0, 65535};
static char image_path[256];
static int display_mode = ModeInvalid;

/* Application state */
static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;
static tll(struct jab_output) outputs;
static struct jab_image image;
static bool running = false;

static const char usage[] = "usage: jab [-hVp] [-c color] [-i image] [-m mode]\n";

static void
noop()
{
	/* This space intentionally left blank */
}

static bool
parse_pixman_color(const char *color, pixman_color_t *result)
{
	int r, g, b;
	int count = sscanf(color, "%02x%02x%02x", &r, &g, &b);

	if (count != 3)
		return false;

	result->red = (double)r * 0xffff / 0xff;
	result->green = (double)g * 0xffff / 0xff;
	result->blue = (double)b * 0xffff / 0xff;

	return true;
}

static inline int
parse_display_mode(const char *mode)
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
jab_output_destroy_surface(struct jab_output *output)
{
	if (output->layer_surface)
		zwlr_layer_surface_v1_destroy(output->layer_surface);
	if (output->surface)
		wl_surface_destroy(output->surface);
}

static void
jab_output_destroy(struct jab_output *output)
{
	jab_output_destroy_surface(output);
	wl_output_release(output->wl_output);
}

static void
render_frame(struct jab_output *output, unsigned int width, unsigned int height)
{
	pixman_image_t *surface_image, *src_image = NULL;

	if (display_mode != ModeInvalid) {
		src_image = pixman_image_create_bits_no_clear(PIXMAN_a8b8g8r8, image.width, image.height,
				(uint32_t *)image.buf, image.width * 4);
		switch (display_mode) {
		case ModeFill: image_fill(src_image, width, height); break;
		case ModeFit: image_fit(src_image, width, height); break;
		case ModeStretch: image_stretch(src_image, width, height); break;
		case ModeCenter: image_center(src_image, width, height); break;
		case ModeTile: image_tile(src_image, width, height); break;
		default: abort(); /* Unreachable */
		}
		if (!pixel_perfect)
			pixman_image_set_filter(src_image, PIXMAN_FILTER_BEST, NULL, 0);
	}

	surface_image = create_surface_image(shm, width, height, output->surface);
	pixman_image_fill_rectangles(PIXMAN_OP_SRC, surface_image, &color, 1,
			&(pixman_rectangle16_t){0, 0, width, height});

	if (display_mode != ModeInvalid) {
		pixman_image_composite32(PIXMAN_OP_OVER, src_image, NULL, surface_image,
				0, 0, 0, 0, 0, 0, width, height);
		pixman_image_unref(src_image);
	}

	wl_surface_commit(output->surface);
	pixman_image_unref(surface_image);
}

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
		uint32_t width, uint32_t height)
{
	struct jab_output *output = data;

	if (output->width == width && output->height == height)
		return;

	output->width = width;
	output->height = height;
	output->configure_serial = serial;
	output->needs_ack = true;
	output->dirty = true;
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
	struct jab_output *output = data;
	tll_foreach(outputs, it)
		if (&it->item == output)
			return jab_output_destroy_surface(output);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

/* TODO: rename */
static void
add_surface_to_output(struct jab_output *output)
{
	struct wl_region *input_region;

	output->surface = wl_compositor_create_surface(compositor);

	/* passthrough input */
	input_region = wl_compositor_create_region(compositor);
	wl_surface_set_input_region(output->surface, input_region);
	wl_region_destroy(input_region);

	output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(layer_shell, output->surface,
			output->wl_output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "background");
	zwlr_layer_surface_v1_set_size(output->layer_surface, 0, 0);
	zwlr_layer_surface_v1_set_anchor(output->layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
	zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, -1);
	zwlr_layer_surface_v1_add_listener(output->layer_surface, &layer_surface_listener, output);
	wl_surface_commit(output->surface);
}

static void
output_done(void *data, struct wl_output *wl_output)
{
	struct jab_output *output = data;
	if (!output->layer_surface)
		add_surface_to_output(output);
}

static void
output_name(void *data, struct wl_output *wl_output, const char *name)
{
	struct jab_output *output = data;
	strncpy(output->name, name, sizeof output->name);
	output->name[sizeof output->name - 1] = '\0';
}

static void
output_description(void *data, struct wl_output *wl_output, const char *description)
{
	struct jab_output *output = data;
	/* wlroots returns the description in the format of `identifier (name)'. If this changes,
	 * this function will need to be updated. However, the name can be omitted, so we check
	 * for that below. */
	int identifier_len = strcspn(description, "(");

	if (identifier_len == strlen(description))
		identifier_len = strlen(description);
	else
		identifier_len--;

	strncpy(output->identifier, description, identifier_len);
	output->identifier[sizeof output->identifier - 1] = '\0';
}

static const struct wl_output_listener output_listener = {
	.geometry = noop,
	.mode = noop,
	.done = output_done,
	.scale = noop,
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
		tll_push_back(outputs, ((struct jab_output){
					.wl_output = wl_registry_bind(registry, name, &wl_output_interface, 4),
					.wl_name = name }));
		output = &tll_back(outputs);
		wl_output_add_listener(output->wl_output, &output_listener, output);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	tll_foreach(outputs, it)
		if (it->item.wl_name == name)
			return jab_output_destroy(&it->item);
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

int
main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE, c;
	opterr = 0;

	while ((c = getopt(argc, argv, "hVpc:i:m:")) != -1)
		switch (c) {
			case 'h':
				fputs(usage, stderr);
				exit(EXIT_SUCCESS);
			case 'V':
				fputs("jab-"VERSION"\n", stderr);
				exit(EXIT_SUCCESS);
			case 'p':
				pixel_perfect = true;
				break;
			case 'c':
				if (!parse_pixman_color(optarg, &color)) {
					fprintf(stderr, "jab: failed to parse color\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'i':
				strncpy(image_path, optarg, sizeof image_path);
				image_path[sizeof image_path - 1] = '\0';
				break;
			case 'm':
				display_mode = parse_display_mode(optarg);
				if (display_mode == ModeInvalid) {
					fprintf(stderr, "jab: failed to parse mode\n");
					exit(EXIT_FAILURE);
				}
				break;
			case '?':
				if (optopt == 'c' || optopt == 'i' || optopt == 'm')
					fprintf(stderr, "jab: option requires argument -- '%c'\n", optopt);
				else
					fprintf(stderr, "jab: unknown option -- '%c'\n", optopt);
				fputs(usage, stderr);
				/* Fallthrough */
			default:
				exit(EXIT_FAILURE);
		}

	if (display_mode != ModeInvalid && image_path[0] != '\0')
		if ((image.buf = stbi_load(image_path, &image.width, &image.height, NULL, 4)) == NULL) {
			fprintf(stderr, "jab: failed to load image: %s\n", stbi_failure_reason());
			goto finish;
		}

	display = wl_display_connect(NULL);
	if (!display) {
		fputs("jab: failed to connect to display\n", stderr);
		goto finish;
	}

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (!compositor || !shm || !layer_shell) {
		fputs("jab: unsupported compositor\n", stderr);
		goto finish;
	}

	ret = EXIT_SUCCESS;
	running = true;
	while (running && wl_display_dispatch(display) != -1) {
		wl_display_flush(display);
		tll_foreach(outputs, it) {
			if (it->item.needs_ack) {
				it->item.needs_ack = false;
				zwlr_layer_surface_v1_ack_configure(it->item.layer_surface,
						it->item.configure_serial);
			}
			if (it->item.dirty) {
				it->item.dirty = false;
				render_frame(&it->item, it->item.width, it->item.height);
			}
		}
	}

finish:
	tll_foreach(outputs, it)
		jab_output_destroy(&it->item);
	tll_free(outputs);
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
