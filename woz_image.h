//
// woz_image.h
//
// Copyright (c) 2021 by Ben Zotto
//
// This module supports (a subset of) WOZ 2.0 disk image format build and write.
// Please see https://applesaucefdc.com/woz/reference2/
//

#ifndef woz_image_h
#define woz_image_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct _woz_chunk {
    char name[4];
    size_t mark;
    size_t buffer_size;
    uint8_t * data;
} woz_chunk;

typedef struct _woz_file {
    woz_chunk * info;
    woz_chunk * tmap;
    woz_chunk * trks;
    woz_chunk * writ;
} woz_file;

woz_file * create_empty_woz_file(void);
void free_woz_file(woz_file * woz);
int write_woz_to_file(woz_file * woz, const char * path);

woz_chunk * create_woz_chunk(const char * name);
void free_chunk(woz_chunk * chunk);
size_t chunk_size_on_disk(woz_chunk * chunk);
void chunk_write_uint8(woz_chunk * chunk, uint8_t u8);
void chunk_write_uint16(woz_chunk * chunk, uint16_t u16);
void chunk_write_uint32(woz_chunk * chunk, uint32_t u32);
void chunk_write_utf8(woz_chunk * chunk, const char * utf8string, int n);
void chunk_write_bytes(woz_chunk * chunk, const uint8_t * bytes, size_t n);
void chunk_set_mark(woz_chunk * chunk, size_t mark);
void chunk_advance_mark(woz_chunk * chunk, int offset);

uint32_t woz_crc32(const void *buf, size_t size);

#endif /* woz_image_h */
