// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <pixman.h>
#include <webp/decode.h>

#include "waywallpaper.h"

pixman_image_t *load_webp(FILE *file) {
	// TODO: error checks for malformed files, etc.
	fseek(file, 0, SEEK_END);

	size_t image_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	uint8_t *file_data = mmap(NULL, image_size, PROT_READ, MAP_PRIVATE, fileno(file), 0);

	int32_t width, height;
	if (!WebPGetInfo(file_data, image_size, &width, &height)) {
		munmap(file_data, image_size);
		return NULL;
	}

	uint32_t stride = width * 4;
	uint8_t *data = mmap(NULL, height * stride, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	WebPDecodeBGRAInto(file_data, image_size, data, height * stride, stride);

	pixman_image_t *image = pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, width, height, (uint32_t *)data, stride);
	pixman_image_set_destroy_function(image, unmap_pixman_image, NULL);

	munmap(file_data, image_size);

	return image;
}
