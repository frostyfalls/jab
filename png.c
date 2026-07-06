#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <pixman.h>
#include <png.h>

#include "waywallpaper.h"

pixman_image_t *load_png(FILE *file) {
	// TODO: error checks
	fseek(file, 0, SEEK_SET);

	uint8_t sig[8];
	fread(sig, 1, 8, file);
	if (!png_check_sig(sig, 8))
		return NULL;

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png_create_info_struct(png);
	setjmp(png_jmpbuf(png));

	png_init_io(png, file);
	png_set_sig_bytes(png, 8);
	png_read_info(png, info);
	png_set_bgr(png);

	uint32_t width = png_get_image_width(png, info);
	uint32_t height = png_get_image_height(png, info);
	png_byte color_type = png_get_color_type(png, info);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

	uint32_t stride = width * 4;
	uint8_t *data = mmap(NULL, height * stride, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	png_bytep *row_pointers = malloc(sizeof(*row_pointers) * height);
	for (uint32_t i = 0; i < height; ++i)
		row_pointers[i] = &data[i * stride];
	png_read_image(png, row_pointers);

	pixman_image_t *image = pixman_image_create_bits_no_clear(PIXMAN_x8r8g8b8, width, height, (uint32_t *)data, stride);
	pixman_image_set_destroy_function(image, unmap_pixman_image, NULL);

	free(row_pointers);
	png_destroy_read_struct(&png, &info, NULL);

	return image;
}
