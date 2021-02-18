//
// buffered_reader.h
//
// Copyright (c) 2021 by Ben Zotto
//
// This module provides a general buffered reader that allows reading values with either
// endianness. Useful for parsing packed formatted file data.
//

#ifndef buffered_reader_h
#define buffered_reader_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// This buffer size can be changed, but don't make it pathologically small (ie < 8 bytes).
// There are assumptions in the logic you will end up violating if you insist on doing so.
#define BUFFER_SIZE 1024

typedef enum _file_endianness {
    file_endianness_little,
    file_endianness_big
} file_endianness;

typedef struct _buffered_reader {
    FILE * file;
    file_endianness endianness;
    size_t total_size;
    size_t offset;
    size_t mark;
    size_t valid;
    uint8_t buffer[BUFFER_SIZE];
} buffered_reader;

buffered_reader * open_buffered_reader(const char * path, file_endianness endianness);
int buffered_reader_ensure_remaining(buffered_reader * reader, size_t ensure);
void buffered_reader_advance_to_offset(buffered_reader * reader, size_t offset);
void close_buffered_reader(buffered_reader * reader);

uint8_t read_uint8(buffered_reader * reader);
uint16_t read_uint16(buffered_reader * reader);
uint32_t read_uint32(buffered_reader * reader);
int8_t read_int8(buffered_reader * reader);
int16_t read_int16(buffered_reader * reader);
int32_t read_int32(buffered_reader * reader);
void read_bytes(buffered_reader * reader, uint8_t * dest, size_t count);

#endif /* buffered_reader_h */
