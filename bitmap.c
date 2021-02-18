//
// bitmap.c
//
// Copyright (c) 2021 by Ben Zotto
//

#include "bitmap.h"
#include <math.h>

static double sRGB_to_linear(double x);
static double linear_to_sRGB(double x);

bitmap * create_bitmap(int width, int height)
{
    bitmap * bitmap = calloc(sizeof(bitmap) + (width * height * 4), 1);
    if (bitmap) {
        bitmap->width = width;
        bitmap->height = height;
    }
    return bitmap;
}

double sample_bitmap_greyscale(bitmap * bitmap, float u, float v)
{
    // u, v are texcoords in the [0, 1] range. tex coords at 1.0 don't overflow, they
    // get clamped to the final value on that axis, and are equivalent to 1-epsilon.
    if (u < 0.0) { u = 0.0; }
    if (u > 1.0) { u = 1.0; }
    if (v < 0.0) { v = 0.0; }
    if (v > 1.0) { v = 1.0; }
    int x = (int)(u * bitmap->width);
    if (x == bitmap->width) { x = bitmap->width - 1; }
    int y = (int)(v * bitmap->height);
    if (y == bitmap->height) { y = bitmap->height - 1; }
    
    // Alpha channel is just ignored in this operation.
    int base = BITMAP_PIXEL_BASE(bitmap, x, y);
    uint8_t r = bitmap->rgba_pixels[base];
    uint8_t g = bitmap->rgba_pixels[base + 1];
    uint8_t b = bitmap->rgba_pixels[base + 2];
    
    double r_linear = sRGB_to_linear(r/255.0);
    double g_linear = sRGB_to_linear(g/255.0);
    double b_linear = sRGB_to_linear(b/255.0);
    double grey_linear = 0.2126 * r_linear + 0.7152 * g_linear + 0.0722 * b_linear;

    return round(linear_to_sRGB(grey_linear));
}

void free_bitmap(bitmap * bitmap)
{
    free(bitmap);
}

//
// Private colorspace gamma conversion, see
// https://en.wikipedia.org/wiki/Grayscale#Converting_color_to_grayscale
//

static
double sRGB_to_linear(double x) {
    if (x < 0.04045) {
        return x / 12.92;
    }
    return pow((x + 0.055) / 1.055, 2.4);
}

static
double linear_to_sRGB(double y) {
    if (y <= 0.0031308) {
        return 12.92 * y;
    }
    return 1.055 * pow(y, 1.0 / 2.4) - 0.055;
}

