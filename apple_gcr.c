//
// apple_gcr.h
//
// Copyright (c) 2021 by Ben Zotto
// Portions of this module are (c) 2018 Thomas Harte (offered under the same LICENSE).
//

#include "apple_gcr.h"
#include <string.h>

#define DOS_VOLUME_NUMBER           254
#define TRACK_LEADER_SYNC_COUNT     64
#define SECTORS_PER_TRACK           16
#define BYTES_PER_SECTOR            256
#define GCR_SECTOR_ENCODED_SIZE     343

static size_t bits_write_byte(uint8_t * buffer, size_t index, int value);
static size_t bits_write_4_and_4(uint8_t * buffer, size_t index, int value);
static size_t bits_write_sync(uint8_t * buffer, size_t index);
static void encode_6_and_2(uint8_t * dest, const uint8_t * src);

//
// Track encoding and writing routines
//

size_t gcr_encode_bits_for_track(uint8_t * dest, uint8_t * src, int track_number, dsk_sector_format sector_format)
{
    size_t bit_index = 0;
    memset(dest, 0, GCR_ENCODED_TRACK_SIZE);

    // Write 64 sync words
    for (int i = 0; i < TRACK_LEADER_SYNC_COUNT; i++) {
        bit_index = bits_write_sync(dest, bit_index);
    }

    // Write out the sectors in physical order. We will select the appopriate logical
    // input data for each physical output sector.
    for (int s = 0; s < SECTORS_PER_TRACK; s++) {

        //
        // Sector header
        //

        // Prologue
        bit_index = bits_write_byte(dest, bit_index, 0xD5);
        bit_index = bits_write_byte(dest, bit_index, 0xAA);
        bit_index = bits_write_byte(dest, bit_index, 0x96);

        // Volume, track, sector and checksum, all in 4-and-4 format
        bit_index = bits_write_4_and_4(dest, bit_index, DOS_VOLUME_NUMBER);
        bit_index = bits_write_4_and_4(dest, bit_index, track_number);
        bit_index = bits_write_4_and_4(dest, bit_index, s);
        bit_index = bits_write_4_and_4(dest, bit_index, DOS_VOLUME_NUMBER ^ track_number ^ s);

        // Epilogue
        bit_index = bits_write_byte(dest, bit_index, 0xDE);
        bit_index = bits_write_byte(dest, bit_index, 0xAA);
        bit_index = bits_write_byte(dest, bit_index, 0xEB);

        // Write 7 sync words.
        for (int i = 0; i < 7; i++) {
            bit_index = bits_write_sync(dest, bit_index);
        }

        //
        // Sector body
        //

        // Prologue
        bit_index = bits_write_byte(dest, bit_index, 0xD5);
        bit_index = bits_write_byte(dest, bit_index, 0xAA);
        bit_index = bits_write_byte(dest, bit_index, 0xAD);

        // Figure out which logical sector goes into this physical sector.
        int logical_sector;
        if (s == 0x0F) {
            logical_sector = 0x0F;
        } else {
            int multiplier = (sector_format == dsk_sector_format_prodos) ? 8 : 7;
            logical_sector = (s * multiplier) % 15;
        }

        // Finally, the actual contents! Encode the buffer, then write them.
        uint8_t encoded_contents[GCR_SECTOR_ENCODED_SIZE];
        encode_6_and_2(encoded_contents, &src[logical_sector * BYTES_PER_SECTOR]);
        for (int i = 0; i < GCR_SECTOR_ENCODED_SIZE; i++) {
            bit_index = bits_write_byte(dest, bit_index, encoded_contents[i]);
        }

        // Epilogue
        bit_index = bits_write_byte(dest, bit_index, 0xDE);
        bit_index = bits_write_byte(dest, bit_index, 0xAA);
        bit_index = bits_write_byte(dest, bit_index, 0xEB);

        // Conclude the track
        if (s < (SECTORS_PER_TRACK - 1)) {
            // Write 16 sync words
            for (int i = 0; i < 16; i++) {
                bit_index = bits_write_sync(dest, bit_index);
            }
        } else {
            bit_index = bits_write_byte(dest, bit_index, 0xFF);
        }
    }

    // Any remaining bytes in the destination buffer will remain cleared to zero and
    // function as padding to the nearest 512-byte block.

    // Return the current bit index, which is equal to the number of valid written bits
    return bit_index;
}

//
// Helper routines.
//

static
size_t bits_write_byte(uint8_t * buffer, size_t index, int value)
{
    size_t shift = index & 7;
    size_t byte_position = index >> 3;

    buffer[byte_position] |= value >> shift;
    if (shift) {
        buffer[byte_position + 1] |= value << (8 - shift);
    }
    
    return index + 8;
}

// Writes a byte in 4-and-4
static
size_t bits_write_4_and_4(uint8_t * buffer, size_t index, int value)
{
    index = bits_write_byte(buffer, index, (value >> 1) | 0xAA);
    index = bits_write_byte(buffer, index, value | 0xAA);
    return index;
}

// Writes a 6-and-2 sync word
static
size_t bits_write_sync(uint8_t * buffer, size_t index)
{
    index = bits_write_byte(buffer, index, 0xFF);
    return index + 2; // Skip two bits, i.e. leave them as 0s.
}

// Encodes a 256-byte sector buffer into a 343 byte 6-and-2 encoding of same
static
void encode_6_and_2(uint8_t * dest, const uint8_t * src)
{
    const uint8_t six_and_two_mapping[] = {
        0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
        0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
        0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
        0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
        0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
        0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
        0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
        0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
    };

    // Fill in byte values: the first 86 bytes contain shuffled
    // and combined copies of the bottom two bits of the sector
    // contents; the 256 bytes afterwards are the remaining
    // six bits.
    const uint8_t bit_reverse[] = {0, 2, 1, 3};
    for (int c = 0; c < 84; c++) {
        dest[c] =
            bit_reverse[src[c] & 3] |
            (bit_reverse[src[c + 86] & 3] << 2) |
            (bit_reverse[src[c + 172] & 3] << 4);
    }
    dest[84] =
        (bit_reverse[src[84] & 3] << 0) |
        (bit_reverse[src[170] & 3] << 2);
    dest[85] =
        (bit_reverse[src[85] & 3] << 0) |
        (bit_reverse[src[171] & 3] << 2);

    for (int c = 0; c < 256; c++) {
        dest[86+c] = src[c] >> 2;
    }

    // Exclusive OR each byte with the one before it.
    dest[342] = dest[341];
    int location = 342;
    while (location > 1) {
        location--;
        dest[location] ^= dest[location-1];
    }

    // Map six-bit values up to full bytes.
    for (int c = 0; c < 343; c++) {
        dest[c] = six_and_two_mapping[dest[c]];
    }
}

