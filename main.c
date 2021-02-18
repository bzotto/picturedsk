//
// main.c
//
// Copyright (c) 2021 by Ben Zotto
//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bmp_bitmap.h"
#include "apple_gcr.h"
#include "woz_image.h"

#define SCREEN_BITMAP_DIMENSION     147
#define SCREEN_BITMAP_STRIDE_BYTES  (SCREEN_BITMAP_DIMENSION / 7)
#define DISPLAY_MESSAGE_OFFSET      177

#define CREATOR_NAME        "PictureDSK"
#define MAX_MESSAGE_LEN     40

#define TRACKS_PER_DISK     46
#define SECTORS_PER_TRACK   16
#define BYTES_PER_SECTOR    256
#define BYTES_PER_TRACK     (SECTORS_PER_TRACK * BYTES_PER_SECTOR)

#define BITS_BLOCKS_PER_TRACK       13
#define BITS_BLOCK_SIZE             512
#define BITS_TRACK_SIZE             (BITS_BLOCKS_PER_TRACK * BITS_BLOCK_SIZE)
#define BITS_SECTOR_CONTENTS_SIZE   343

#define DOS_VOLUME_NUMBER           254
#define TRACK_LEADER_SYNC_COUNT     64

//
// Helper types and routines.
//

typedef struct _track_data {
    size_t data_length;
    int block_count;
    uint8_t data[0];
} track_data;

static track_data * create_track_data(size_t length);
static void free_track_data(track_data * data);

static uint8_t boot_1_sector_0[BYTES_PER_SECTOR];
static uint8_t boot_2_sector_F[BYTES_PER_SECTOR];

//
// Main program.
//

int main(int argc, const char * argv[])
{
    if (argc < 3 || argc > 4) {
        printf("USAGE: picturedsk image.bmp output.woz [message] \n");
        return -1;
    }

    // Load the input bitmap.
    bitmap * image = load_bmp_into_bitmap(argv[1]);
    if (!image) {
        // That routine will print its own granular error.
        return -2;
    }
    
    //
    // Sample the bitmap to create a version in the Apple high-res format.
    //
    
    uint8_t a2_high_res_image[SCREEN_BITMAP_STRIDE_BYTES * SCREEN_BITMAP_DIMENSION];
    uint8_t * a2_dest_ptr = &a2_high_res_image[0];
    uint8_t shiftreg = 0x80;
    int shiftreg_valid = 0;
    for (int y = 0; y < SCREEN_BITMAP_DIMENSION; y++) {
        for (int x = 0; x < SCREEN_BITMAP_DIMENSION; x++) {
            float u = x / (float)SCREEN_BITMAP_DIMENSION;
            float v = y / (float)SCREEN_BITMAP_DIMENSION;
            double grey = sample_bitmap_greyscale(image, u, v);
            uint8_t bit = 1 << shiftreg_valid;
            if (grey >= 0.5) {
                shiftreg |= bit;
            }
            if (++shiftreg_valid == 7) {
                *a2_dest_ptr++ = shiftreg;
                shiftreg = 0x80;
                shiftreg_valid = 0;
            }
        }
    }
    
    //
    // Build the .DSK-format data for the first (and sole valid) track on the disk.
    // Shuffle the image data into the interleaved disk sectors, so it'll end
    // up loaded consecutively at $B100. The boot1 boot loader goes in
    // sector 0, and the boot2 code goes in sector F.
    //

    uint8_t track_0[SECTORS_PER_TRACK * BYTES_PER_SECTOR];
    memset(track_0, 0, SECTORS_PER_TRACK * BYTES_PER_SECTOR);
    memcpy(&track_0[0x000], boot_1_sector_0, BYTES_PER_SECTOR);
    memcpy(&track_0[0x800], &a2_high_res_image[0x000], BYTES_PER_SECTOR);
    memcpy(&track_0[0x100], &a2_high_res_image[0x100], BYTES_PER_SECTOR);
    memcpy(&track_0[0x900], &a2_high_res_image[0x200], BYTES_PER_SECTOR);
    memcpy(&track_0[0x200], &a2_high_res_image[0x300], BYTES_PER_SECTOR);
    memcpy(&track_0[0xA00], &a2_high_res_image[0x400], BYTES_PER_SECTOR);
    memcpy(&track_0[0x300], &a2_high_res_image[0x500], BYTES_PER_SECTOR);
    memcpy(&track_0[0xB00], &a2_high_res_image[0x600], BYTES_PER_SECTOR);
    memcpy(&track_0[0x400], &a2_high_res_image[0x700], BYTES_PER_SECTOR);
    memcpy(&track_0[0xC00], &a2_high_res_image[0x800], BYTES_PER_SECTOR);
    memcpy(&track_0[0x500], &a2_high_res_image[0x900], BYTES_PER_SECTOR);
    memcpy(&track_0[0xD00], &a2_high_res_image[0xA00], BYTES_PER_SECTOR);
    memcpy(&track_0[0x600], &a2_high_res_image[0xB00], BYTES_PER_SECTOR);
    memcpy(&track_0[0xE00], &a2_high_res_image[0xC00], 15); // This is how many valid bytes are left in the image.
    memcpy(&track_0[0xF00], boot_2_sector_F, BYTES_PER_SECTOR);
    
    // Fixup the custom display string if one is supplied
    if (argc == 4) {
        int message_len = (int)strlen(argv[3]);
        if (message_len > MAX_MESSAGE_LEN) message_len = MAX_MESSAGE_LEN;
        char * message_base = (char *)&track_0[0xF00 + DISPLAY_MESSAGE_OFFSET];
        for (int i = 0; i < message_len; i++) {
            char ch = argv[3][i];
            if (ch >= 'a' && ch <= 'z') {
                ch -= 0x20;
            }
            if (ch < ' ' || ch > '_') {
                ch = ' ';
            }
            message_base[i] = ch;
        }
        // Two newlines and the terminal nul.
        message_base[message_len] = 0x0D;
        message_base[message_len + 1] = 0x0D;
        message_base[message_len + 2] = 0x00;
    }
    
    //
    // Prepare the raw data for all of the disk's tracks.
    //
    
    track_data * tracks[TRACKS_PER_DISK];
    
    // Encode the one "valid" outer track.
    tracks[0] = create_track_data(BITS_TRACK_SIZE);
    gcr_encode_bits_for_track(tracks[0]->data, track_0, 0, dsk_sector_format_dos_3_3);
    
    // Encode the remaining tracks by using a polar coordinate texture sampling of the
    // input bitmap image. All tracks on the disk are the same size (13 WOZ blocks).
    
    // This is based on the output PNG files from the current version of Applesauce.
    float radius_per_track = (0.5 - 0.1415) / (float)(TRACKS_PER_DISK - 1);
    double arc_segment = (2.0 * M_PI) / (double)BITS_TRACK_SIZE;

    for (int i = 1; i < TRACKS_PER_DISK; i++) {
        float r = 0.5 - ((i - 1) * radius_per_track);
        tracks[i] = create_track_data(BITS_TRACK_SIZE);
        for (int track_byte_index = 0; track_byte_index < BITS_TRACK_SIZE; track_byte_index++) {
            // Get a set of (u, v) texture map points from the angle.
            float u = r * cosf(M_PI_2 + arc_segment * (BITS_TRACK_SIZE - track_byte_index));
            float v = r * sinf(M_PI_2 + arc_segment * (BITS_TRACK_SIZE - track_byte_index));
            // Translate (u,v) from the center, to the origin.
            u += 0.5;
            v = 0.5 - v;
            double gray = sample_bitmap_greyscale(image, u, v);
            tracks[i]->data[track_byte_index] = (gray > 0.5) ? 0xFF : 0x96;
        }
    }
    
    //
    // Build the WOZ file from the track data.
    //
    
    woz_file * woz = create_empty_woz_file();
    if (!woz) {
        printf("Out of memory.\n");
        return -3;
    }
    
    // Build INFO chunk
    chunk_write_uint8(woz->info, 2); // INFO v2
    chunk_write_uint8(woz->info, 1); // 5.25" image
    chunk_write_uint8(woz->info, 1); // Write protected
    chunk_write_uint8(woz->info, 1); // Synchronized
    chunk_write_uint8(woz->info, 1); // Cleaned
    chunk_write_utf8(woz->info, CREATOR_NAME, 32); // Creator
    chunk_write_uint8(woz->info, 1); // 1 disk side
    chunk_write_uint8(woz->info, 1); // 16-sector format
    chunk_write_uint8(woz->info, 32); // 4uS standard bit timing
    chunk_write_uint16(woz->info, 0x7F); // Should work on the whole ][ series (?)
    chunk_write_uint16(woz->info, 64); // I think this requires 64k (?)
    chunk_write_uint16(woz->info, BITS_BLOCKS_PER_TRACK); // Largest track size (all are same)
    
    // Build TMAP chunk
    //
    // Track 0 appears at its normal location with its normal bleed-over into 0.25, with the
    // normal gap at 0.5. The rest of the tracks are all side-by-each starting at position 1.0,
    // with no gap between the sets of three "detected" bits. The rest of the chunk gets the 0xFF
    // nothing-marker (not zeros which would indicate something else).
    chunk_write_uint8(woz->tmap, 0);
    chunk_write_uint8(woz->tmap, 0);
    chunk_write_uint8(woz->tmap, 0xFF);
    int tmap_nominal_track = 0; // start at 0 so the first loop increments to 1
    for (int i = 3; i < 160; i++) {
        if (i % 3 == 0) tmap_nominal_track++;
        if (tmap_nominal_track < TRACKS_PER_DISK) {
            chunk_write_uint8(woz->tmap, tmap_nominal_track);
        } else {
            chunk_write_uint8(woz->tmap, 0xFF);
        }
    }
    
    // Build TRKS chunk
    // !!! starting_block is relative to the start of the file !!! This means we rely on
    // writing the chunks in a fixed order up to this point (INFO, TMAP, TRKS, ...).
    uint16_t starting_block = 3;
    for (int i = 0 ; i < TRACKS_PER_DISK; i++) {
        chunk_write_uint16(woz->trks, starting_block);
        chunk_write_uint16(woz->trks, tracks[i]->block_count);
        chunk_write_uint32(woz->trks, (uint32_t)tracks[i]->data_length * 8);
        starting_block += tracks[i]->block_count;
    }
    chunk_set_mark(woz->trks, 1280);
    for (int i = 0 ; i < TRACKS_PER_DISK; i++) {
        chunk_write_bytes(woz->trks, tracks[i]->data, tracks[i]->data_length);
        int empty_padding_length = (int)((tracks[i]->block_count * BITS_BLOCK_SIZE) - tracks[i]->data_length);
        chunk_advance_mark(woz->trks, empty_padding_length);
    }

    // Build WRIT chunk
    int subtrack_index = 0;
    for (int i = 0; i < TRACKS_PER_DISK; i++) {
        // Track 0 is written at subtrack 0.0. skip to track 1.0 for track 1, but then every 3
        // after that.
        chunk_write_uint8(woz->writ, subtrack_index);
        subtrack_index += ((i == 0) ? 4 : 3);
        chunk_write_uint8(woz->writ, 1);        // 1 command in this set
        chunk_write_uint8(woz->writ, 0x01);     // Clear first
        chunk_write_uint8(woz->writ, 0);        // Reserved (0)
        size_t length_for_crc = (tracks[i]->data_length + 7) / 8;
        uint32_t crc = woz_crc32(tracks[i]->data, length_for_crc);
        chunk_write_uint32(woz->writ, crc);     // BITS checksum
        chunk_write_uint32(woz->writ, 0);       // Don't write leader
        chunk_write_uint32(woz->writ, (uint32_t)tracks[i]->data_length * 8);
        chunk_write_uint8(woz->writ, 0x00);     // Leader nibble
        chunk_write_uint8(woz->writ, 0);        // Leader nibble count
        chunk_write_uint8(woz->writ, 0);        // Leader count
        chunk_write_uint8(woz->writ, 0);        // Reserved (0)
    }
    
    //
    // We have a complete WOZ built up in parts. Write the whole thing out to a single
    // file buffer for writing to disk.
    //
    
    write_woz_to_file(woz, argv[2]);
    
    // Cleanup like a good boy scout
    free_woz_file(woz);
    free_bitmap(image);
    for (int i = 0; i < TRACKS_PER_DISK; i++) {
        free_track_data(tracks[i]);
    }

    return 0;
}

//
//
//

static
track_data * create_track_data(size_t length)
{
    track_data * data = calloc(1, sizeof(track_data) + length);
    if (data) {
        data->data_length = length;
        int block_count = (int)(length / BITS_BLOCK_SIZE);
        if (length % BITS_BLOCK_SIZE > 0) {
            block_count++;
        }
        data->block_count = block_count;
    }
    return data;
}

static
void free_track_data(track_data * data)
{
    free(data);
}

static
uint8_t boot_1_sector_0[BYTES_PER_SECTOR] = {
    0x01, 0xA5, 0x27, 0xC9, 0x09, 0xD0, 0x18, 0xA5, 0x2B, 0x4A, 0x4A, 0x4A, 0x4A, 0x09, 0xC0, 0x85,
    0x3F, 0xA9, 0x5C, 0x85, 0x3E, 0x18, 0xAD, 0x5C, 0x08, 0x6D, 0x5D, 0x08, 0x8D, 0x5C, 0x08, 0xAE,
    0x5D, 0x08, 0x30, 0x15, 0xBD, 0x4B, 0x08, 0x85, 0x5D, 0xCE, 0x5D, 0x08, 0xAD, 0x5C, 0x08, 0x85,
    0x27, 0xCE, 0x5C, 0x08, 0xA6, 0x2B, 0x6C, 0x3E, 0x00, 0xEE, 0x5C, 0x08, 0xEE, 0x5C, 0x08, 0x20,
    0x89, 0xFE, 0x20, 0x93, 0xFE, 0x20, 0x2F, 0xFB, 0x4C, 0x00, 0xB0, 0x00, 0x0D, 0x0B, 0x09, 0x07,
    0x05, 0x03, 0x01, 0x0E, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x0F, 0x00, 0xB0, 0x0E, 0xB0, 0x0E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x50, 0x69, 0x63, 0x74, 0x75, 0x72,
    0x65, 0x44, 0x53, 0x4B, 0x20, 0x28, 0x74, 0x6D, 0x29, 0x20, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x43, 0x6F, 0x70, 0x79, 0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x28, 0x63, 0x29, 0x20, 0x42, 0x65,
    0x6E, 0x20, 0x5A, 0x6F, 0x74, 0x74, 0x6F, 0x20, 0x32, 0x30, 0x32, 0x31, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static
uint8_t boot_2_sector_F[BYTES_PER_SECTOR] = {
    0xA2, 0x60, 0xBD, 0x88, 0xC0, 0xA2, 0x50, 0xBD, 0x88, 0xC0, 0xA9, 0x17, 0x85, 0x25, 0x20, 0xE2,
    0xF3, 0xA2, 0x07, 0x20, 0xF0, 0xF6, 0x20, 0x57, 0xF4, 0x20, 0xF6, 0xF3, 0xA9, 0xB1, 0x85, 0x09,
    0xA9, 0x00, 0x85, 0x08, 0x85, 0xFB, 0xAE, 0x52, 0xB0, 0x20, 0x53, 0xB0, 0xA0, 0x09, 0x84, 0xFA,
    0xA4, 0xFB, 0xB1, 0x08, 0xC8, 0xD0, 0x03, 0xEE, 0x09, 0x00, 0x84, 0xFB, 0xA4, 0xFA, 0x91, 0x06,
    0xC8, 0xC0, 0x1E, 0xD0, 0xE9, 0xEE, 0x52, 0xB0, 0xA0, 0x99, 0xCC, 0x52, 0xB0, 0xF0, 0x4E, 0x4C,
    0x26, 0xB0, 0x06, 0x8A, 0x4A, 0x4A, 0x4A, 0x18, 0x0A, 0xA8, 0xB9, 0x75, 0xB0, 0x48, 0xC8, 0xB9,
    0x75, 0xB0, 0x48, 0x8A, 0x29, 0x07, 0x18, 0x0A, 0x0A, 0x85, 0x07, 0x68, 0x18, 0x65, 0x07, 0x85,
    0x07, 0x68, 0x85, 0x06, 0x60, 0x00, 0x20, 0x80, 0x20, 0x00, 0x21, 0x80, 0x21, 0x00, 0x22, 0x80,
    0x22, 0x00, 0x23, 0x80, 0x23, 0x28, 0x20, 0xA8, 0x20, 0x28, 0x21, 0xA8, 0x21, 0x28, 0x22, 0xA8,
    0x22, 0x28, 0x23, 0xA8, 0x23, 0x50, 0x20, 0xD0, 0x20, 0x50, 0x21, 0xD0, 0x21, 0xA2, 0x00, 0xBD,
    0xB0, 0xB0, 0xF0, 0x09, 0x09, 0x80, 0x20, 0xF0, 0xFD, 0xE8, 0x4C, 0x9F, 0xB0, 0x4C, 0xAD, 0xB0,
    0x0A, 0x46, 0x4C, 0x55, 0x58, 0x2D, 0x49, 0x4D, 0x41, 0x47, 0x45, 0x20, 0x54, 0x48, 0x49, 0x53,
    0x20, 0x44, 0x49, 0x53, 0x4B, 0x20, 0x46, 0x4F, 0x52, 0x20, 0x41, 0x20, 0x53, 0x55, 0x52, 0x50,
    0x52, 0x49, 0x53, 0x45, 0x0D, 0x3D, 0x29, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x5A
};
