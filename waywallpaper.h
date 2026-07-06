#ifndef _WAYWALLPAPER_H
#define _WAYWALLPAPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <pixman.h>

int allocate_shm_file(size_t size);
void unmap_pixman_image(pixman_image_t *image, void *data);
#ifdef WW_HAVE_PNG
pixman_image_t *load_png(FILE *file);
#endif  // WW_HAVE_PNG
#ifdef WW_HAVE_JPEG
pixman_image_t *load_jpeg(FILE *file);
#endif  // WW_HAVE_JPEG

#endif  // _WAYWALLPAPER_H
