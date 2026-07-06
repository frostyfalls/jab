// SPDX-License-Identifier: MIT

#ifndef _WAYWALLPAPER_H
#define _WAYWALLPAPER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <pixman.h>

extern bool have_png;
extern bool have_jpeg;
extern bool have_webp;

int allocate_shm_file(size_t size);
void unmap_pixman_image(pixman_image_t *image, void *data);

#ifdef WW_HAVE_PNG
pixman_image_t *load_png(FILE *file);
#endif  // WW_HAVE_PNG

#ifdef WW_HAVE_JPEG
pixman_image_t *load_jpeg(FILE *file);
#endif  // WW_HAVE_JPEG

#ifdef WW_HAVE_WEBP
pixman_image_t *load_webp(FILE *file);
#endif  // WW_HAVE_WEBP

#endif  // _WAYWALLPAPER_H
