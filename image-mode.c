#include <math.h>
#include <pixman.h>
#include <stdbool.h>
#include <stddef.h>

static void
image_fit_or_fill(pixman_image_t *src, int width, int height, bool fill)
{
	int src_width = pixman_image_get_width(src), src_height = pixman_image_get_height(src);
	pixman_transform_t t;
	double sx, sy, s;

	sx = (double)width / src_width;
	sy = (double)height / src_height;
	s = fill ? fmax(sx, sy) : fmin(sx, sy);

	pixman_transform_init_scale(&t, pixman_double_to_fixed(1 / s), pixman_double_to_fixed(1 / s));
	pixman_transform_translate(&t, NULL, pixman_double_to_fixed((src_width - width / s) / 2), pixman_double_to_fixed((src_height - height / s) / 2));
	pixman_image_set_transform(src, &t);
}

void
image_fill(pixman_image_t *src, int width, int height)
{
	image_fit_or_fill(src, width, height, true);
}

void
image_fit(pixman_image_t *src, int width, int height)
{
	image_fit_or_fill(src, width, height, false);
}

void
image_stretch(pixman_image_t *src, int width, int height)
{
	int src_width = pixman_image_get_width(src), src_height = pixman_image_get_height(src);
	pixman_transform_t t;
	double sx, sy;

	sx = (double)width / src_width;
	sy = (double)height / src_height;

	pixman_transform_init_scale(&t, pixman_double_to_fixed(1 / sx), pixman_double_to_fixed(1 / sy));
	pixman_image_set_transform(src, &t);
}

void
image_center(pixman_image_t *src, int width, int height)
{
	int src_width = pixman_image_get_width(src), src_height = pixman_image_get_height(src);
	pixman_transform_t t;

	pixman_transform_init_translate(&t, pixman_int_to_fixed((src_width - width) / 2), pixman_int_to_fixed((src_height - height) / 2));
	pixman_image_set_transform(src, &t);
}

void
image_tile(pixman_image_t *src, int width, int height)
{
	pixman_image_set_repeat(src, PIXMAN_REPEAT_NORMAL);
}
