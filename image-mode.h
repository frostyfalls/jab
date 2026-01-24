#ifndef IMAGE_MODE_H
#define IMAGE_MODE_H

#include <pixman.h>

void image_fill(pixman_image_t *src, int width, int height);
void image_fit(pixman_image_t *src, int width, int height);
void image_stretch(pixman_image_t *src, int width, int height);
void image_center(pixman_image_t *src, int width, int height);
void image_tile(pixman_image_t *src, int width, int height);

#endif /* IMAGE_MODE_H */
