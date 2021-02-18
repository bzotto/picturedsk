//
// apple_gcr.h
//
// Copyright (c) 2021 by Ben Zotto
// Portions of this module are (c) 2018 Thomas Harte (offered under the same LICENSE).
//
// This module implements Apple Group Coded Recording (GCR) encoding for floppy disk data.
//

#ifndef apple_gcr_h
#define apple_gcr_h

#include <stdio.h>
#include <stdint.h>

#define GCR_RAW_TRACK_SIZE          (16 * 256)  // Input to encode is expected to be this size
#define GCR_ENCODED_TRACK_SIZE      (13 * 512)  // Output buffer should be at least this large

typedef enum _dsk_sector_format {
    dsk_sector_format_dos_3_3 = 0,
    dsk_sector_format_prodos = 1
} dsk_sector_format;

size_t gcr_encode_bits_for_track(uint8_t * dest, uint8_t * src, int track_number, dsk_sector_format sector_format);

#endif /* apple_gcr_h */
