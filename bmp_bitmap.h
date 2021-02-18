//
// bmp_bitmap.h
//
// Copyright (c) 2021 by Ben Zotto
//
// This module provides basic functionality for reading Windows-style BMP bitmap
// image files and producing a plain RGBA buffer of pixel values. Only a subset of
// BMP formats are supported: 1, 4, 8, 24, 32 bits per pixel, uncompressed, in the
// BMP v3 or v4 file format styles. This covers most standard generic BMP conversion
// output.
//

#ifndef bmp_bitmap_h
#define bmp_bitmap_h

#include <stdio.h>
#include "bitmap.h"

bitmap * load_bmp_into_bitmap(const char * bmp_path);

#endif /* bmp_bitmap_h */
