//
// buffered_reader.c
//
// Copyright (c) 2021 by Ben Zotto
//

#include "buffered_reader.h"

//
// Private declarations
//

static size_t ensure_minimum_bytes_available(buffered_reader * reader, size_t count);

//
// Public routines
//

buffered_reader * open_buffered_reader(const char * path, file_endianness endianness)
{
    FILE * file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    
    buffered_reader * reader = malloc(sizeof(buffered_reader));
    if (!reader) {
        fclose(file);
        return NULL;
    }
    
    fseek(file, 0L, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    reader->file = file;
    reader->endianness = endianness;
    reader->total_size = size;
    reader->offset = 0;
    reader->mark = 0;
    
    reader->valid = fread(&reader->buffer[0], 1, BUFFER_SIZE, file);
    
    return reader;
}

int buffered_reader_ensure_remaining(buffered_reader * reader, size_t ensure)
{
    return (reader->total_size - (reader->offset + reader->mark)) >= ensure;
}

void buffered_reader_advance_to_offset(buffered_reader * reader, size_t offset)
{
    if (offset > reader->total_size) {
        // Invalid offset
        return;
    }
    if (offset <= reader->offset + reader->mark) {
        // We don't rewind this reader, so if the proposed offset is earlier
        // than the mark, ignore this request.
        return;
    }
    // If the requested offset is within the valid buffer, just move the mark.
    if (offset < reader->offset + reader->valid) {
        reader->mark = offset - reader->offset;
        return;
    }
    // Otherwise dump all the currently buffered, bytes and move the mark and
    // offset appropriately. Seek the file handle to the new offset.
    fseek(reader->file, offset, SEEK_SET);
    reader->offset = offset;
    reader->mark = 0;
    reader->valid = 0;
}

void close_buffered_reader(buffered_reader * reader)
{
    fclose(reader->file);
    free(reader);
}

uint8_t read_uint8(buffered_reader * reader)
{
    if (!ensure_minimum_bytes_available(reader, sizeof(uint8_t))) {
        return 0;
    }
    uint8_t u8 = reader->buffer[reader->mark++];
    return u8;
}

uint16_t read_uint16(buffered_reader * reader)
{
    if (!ensure_minimum_bytes_available(reader, sizeof(uint16_t))) {
        return 0;
    }

    uint8_t one = reader->buffer[reader->mark++];
    uint8_t two = reader->buffer[reader->mark++];
    uint16_t u16;
    if (reader->endianness == file_endianness_little) {
        u16 = (two << 8) | one;
    } else {
        u16 = (one << 8) | two;
    }
    return u16;
}

uint32_t read_uint32(buffered_reader * reader)
{
    if (!ensure_minimum_bytes_available(reader, sizeof(uint32_t))) {
        return 0;
    }
    uint8_t one = reader->buffer[reader->mark++];
    uint8_t two = reader->buffer[reader->mark++];
    uint8_t three = reader->buffer[reader->mark++];
    uint8_t four = reader->buffer[reader->mark++];
    uint32_t u32;
    if (reader->endianness == file_endianness_little) {
        u32 = (four << 24) | (three << 16) | (two << 8) | one;
    } else {
        u32 = (one << 24) | (two << 16) | (three << 8) | four;
    }
    return u32;
}

int8_t read_int8(buffered_reader * reader)
{
    return read_uint8(reader);
}

int16_t read_int16(buffered_reader * reader)
{
    return (int16_t)read_uint16(reader);
}

int32_t read_int32(buffered_reader * reader)
{
    return (int32_t)read_uint32(reader);
}

void read_bytes(buffered_reader * reader, uint8_t * dest, size_t count)
{
    // Copy out anything buffered that is still available
    size_t remaining = reader->valid - reader->mark;
    if (remaining > count) {
        memcpy(dest, &reader->buffer[reader->mark], count);
        reader->mark += count;
        return;
    }
    memcpy(dest, &reader->buffer[reader->mark], remaining);
    reader->offset += reader->valid;
    reader->mark = 0;
    reader->valid = 0;
    reader->offset += fread(dest + remaining, 1, count - remaining, reader->file);
}

static
size_t ensure_minimum_bytes_available(buffered_reader * reader, size_t count)
{
    size_t remaining = reader->valid - reader->mark;
    if (remaining >= count) {
        return remaining;
    }
    // Are there enough in the whole file to fulfill the request?
    if (reader->total_size - (reader->offset + reader->mark) < count) {
        return 0;
    }
    // Shift remaining valid bytes to the top of the buffer, and refill the rest.
    memmove(&reader->buffer[0], &reader->buffer[reader->mark], remaining);
    reader->offset += reader->mark;
    reader->valid = remaining;
    reader->mark = 0;
    reader->valid += fread(&reader->buffer[reader->valid], 1, BUFFER_SIZE - reader->valid, reader->file);
    return reader->valid;
}
