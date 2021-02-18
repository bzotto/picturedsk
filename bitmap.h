//
// bitmap.h
//
// Copyright (c) 2021 by Ben Zotto
//
// This module provides a generic RGBA "bitmap" object, with the ability to sample
// the image in the manner of a texture map.
//

#ifndef bitmap_h
#define bitmap_h

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//
// Generic bitmap structure, in basic y rows of x pixels, stored as packed RBGA quads.
//

typedef struct _bitmap {
    int width;
    int height;
    uint8_t rgba_pixels[0];
} bitmap;

#define BITMAP_BYTES_PER_LINE(b)    ((b)->width * 4)
#define BITMAP_PIXEL_BASE(b, x, y)  (((y) * BITMAP_BYTES_PER_LINE(b)) + (x) * 4)

bitmap * create_bitmap(int width, int height);
double sample_bitmap_greyscale(bitmap * bitmap, float u, float v);
void free_bitmap(bitmap * bitmap);

#endif /* bitmap_h */
