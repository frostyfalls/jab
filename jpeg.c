// SPDX-License-Identifier: MIT

#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <setjmp.h>

#include <jpeglib.h>
#include <pixman.h>

#include "waywallpaper.h"

struct my_error_mgr {
	struct jpeg_error_mgr mgr;
	jmp_buf setjmp_buffer;
};

static void jpeg_error_exit(j_common_ptr cinfo) {
	struct my_error_mgr *jerr = (struct my_error_mgr*)cinfo->err;
	longjmp(jerr->setjmp_buffer, 1);
}

pixman_image_t *load_jpeg(FILE *file) {
	// TODO: better error handling, cleanup data
	fseek(file, 0, SEEK_SET);

	struct jpeg_decompress_struct cinfo = {0};
	struct my_error_mgr jerr = {0};

	cinfo.err = jpeg_std_error(&jerr.mgr);
	jerr.mgr.error_exit = jpeg_error_exit;

	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		return NULL;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, file);
	jpeg_read_header(&cinfo, true);
	jpeg_calc_output_dimensions(&cinfo);
	cinfo.out_color_space = JCS_EXT_BGRA;

	uint32_t width = cinfo.output_width;
	uint32_t height = cinfo.output_height;
	uint32_t stride = width * 4;

	uint8_t *data = mmap(NULL, height * stride, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	jpeg_start_decompress(&cinfo);

	if (cinfo.output_components != 4)
		return NULL;
	for (uint32_t i = 0; cinfo.output_scanline < height; ++i) {
		uint8_t *row = &data[i * stride];
		jpeg_read_scanlines(&cinfo, (JSAMPARRAY)&row, 1);
	}

	pixman_image_t *image = pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, width, height, (uint32_t *)data, stride);
	pixman_image_set_destroy_function(image, unmap_pixman_image, NULL);

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return image;
}
