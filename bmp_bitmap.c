//
// bmp_bitmap.c
//
// Copyright (c) 2021 by Ben Zotto
//

#include <stdint.h>
#include "bmp_bitmap.h"
#include "buffered_reader.h"

typedef struct _bmp_file_header {
    uint16_t file_type;
    uint32_t file_size;
    uint16_t reserved0;
    uint16_t reserved1;
    uint32_t bitmap_offset;
} bmp_file_header;

typedef enum _bmp_compression {
    bmp_compression_none,
    bmp_compression_rle8,
    bmp_compression_rle4,
    bmp_compression_bitfields
} bmp_compression;

#define BMP_HEADER_SIZE_V3      40
#define BMP_HEADER_SIZE_V4      108
#define BMP_HEADER_SIZE_V5      124

typedef struct _bmp_header {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    bmp_compression compression;
    uint32_t size_of_bitmap;
    int32_t horz_resolution;
    int32_t vert_resolution;
    uint32_t colors_used;
    uint32_t colors_important;
    // Fields below this line were added in v4 (Win95/NT4)
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;
    uint32_t cs_type;
    int32_t red_x;
    int32_t red_y;
    int32_t red_z;
    int32_t green_x;
    int32_t green_y;
    int32_t green_z;
    int32_t blue_x;
    int32_t blue_y;
    int32_t blue_z;
    uint32_t gamma_red;
    uint32_t gamma_green;
    uint32_t gamma_blue;
    // Fields below this line were added in v5 (Win98/NT5)
    uint32_t intent;
    uint32_t profile_data;
    uint32_t profile_size;
    uint32_t reserved;
} bmp_header;

typedef struct _bmp_palette_element {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} bmp_palette_element;

bitmap * load_bmp_into_bitmap(const char * bmp_path)
{
    bitmap * bitmap = NULL;
    uint8_t * raw_bitmap_data = NULL;
    
    buffered_reader * reader = open_buffered_reader(bmp_path, file_endianness_little);
    if (!reader) {
        printf("Could not open file %s\n", bmp_path);
        return NULL;
    }

    // Ensure the file header
    if (!buffered_reader_ensure_remaining(reader, 18)) {
        printf("Invalid BMP file\n");
        goto Error;
    }
    
    bmp_file_header file_header;
    file_header.file_type = read_uint16(reader);
    file_header.file_size = read_uint32(reader);
    file_header.reserved0 = read_uint16(reader);
    file_header.reserved1 = read_uint16(reader);
    file_header.bitmap_offset = read_uint32(reader);
    
    // Ensure the entire file size
    if (!buffered_reader_ensure_remaining(reader, file_header.file_size - 18)) {
        printf("Invalid BMP file\n");
        goto Error;
    }
    
    // "BM" (as chars, not a little-endian uint16, so compare to a flipped version)
    if (file_header.file_type != 0x4D42 || file_header.file_size != reader->total_size) {
        printf("Invalid BMP file\n");
        goto Error;
    }
        
    uint32_t bitmap_header_size = read_uint32(reader);
    if (bitmap_header_size != BMP_HEADER_SIZE_V3 &&
        bitmap_header_size != BMP_HEADER_SIZE_V4 &&
        bitmap_header_size != BMP_HEADER_SIZE_V5) {
        printf("Unsupported BMP version\n");
        goto Error;
    }
    
    bmp_header header;
    
    // Read the v3 fields
    header.size = bitmap_header_size;
    header.width = read_int32(reader);
    header.height = read_int32(reader);
    header.planes = read_uint16(reader);
    header.bits_per_pixel = read_uint16(reader);
    header.compression = read_uint32(reader);
    header.size_of_bitmap = read_uint32(reader);
    header.horz_resolution = read_int32(reader);
    header.vert_resolution = read_int32(reader);
    header.colors_used = read_uint32(reader);
    header.colors_important = read_uint32(reader);
    
    // v3 bitmaps can specify bitfield encoding, in which case these RGB mask
    // fields appear immediately after the header.
    if (bitmap_header_size == BMP_HEADER_SIZE_V3 &&
        header.compression == bmp_compression_bitfields ) {
        header.red_mask = read_uint32(reader);
        header.green_mask = read_uint32(reader);
        header.blue_mask = read_uint32(reader);
        header.alpha_mask = 0; // no alpha mask in v3 bitfields.
    }
    
    // Read v4 fields
    if (bitmap_header_size >= BMP_HEADER_SIZE_V4) {
        header.red_mask = read_uint32(reader);
        header.green_mask = read_uint32(reader);
        header.blue_mask = read_uint32(reader);
        header.alpha_mask = read_uint32(reader);
        header.cs_type = read_uint32(reader);
        header.red_x = read_int32(reader);
        header.red_y = read_int32(reader);
        header.red_z = read_int32(reader);
        header.green_x = read_int32(reader);
        header.green_y = read_int32(reader);
        header.green_z = read_int32(reader);
        header.blue_x = read_int32(reader);
        header.blue_y = read_int32(reader);
        header.blue_z = read_int32(reader);
        header.gamma_red = read_uint32(reader);
        header.gamma_green = read_uint32(reader);
        header.gamma_blue = read_uint32(reader);
    }
    
    // Read v5 fields
    if (bitmap_header_size == BMP_HEADER_SIZE_V5) {
        header.intent = read_uint32(reader);
        header.profile_data = read_uint32(reader);
        header.profile_size = read_uint32(reader);
        header.reserved = read_uint32(reader);
    }
    
    if (header.bits_per_pixel != 1 && header.bits_per_pixel != 4 &&
        header.bits_per_pixel != 8 && header.bits_per_pixel != 24 &&
        header.bits_per_pixel != 32) {
        printf("%d-bit BMP not supported\n", header.bits_per_pixel);
        goto Error;
    }
    
    // Currently we only suppport uncompressed bitmaps, plus one (the only?) common
    // 32-bit "bitfields" format.
    if (header.compression == bmp_compression_bitfields) {
        if (header.bits_per_pixel != 32) {
            printf("Unsupported BMP format (%d-bit bitfields)\n", header.bits_per_pixel);
            goto Error;
        }
        if (header.red_mask != 0x00FF0000 || header.green_mask != 0x0000FF00 ||
            header.blue_mask != 0x000000FF || header.alpha_mask != 0xFF000000) {
            printf("Unsupported BMP format (unordered bitfields)\n");
            goto Error;
        }
    } else if (header.compression != bmp_compression_none) {
        printf("Only uncompressed BMP formats supported\n");
        goto Error;
    }
    
    // Load the palette if applicable.
    int palette_entries = 0;
    if (header.bits_per_pixel < 16) {
        if (header.colors_used == 0) {
            palette_entries = 1 << header.bits_per_pixel;
        } else {
            palette_entries = header.colors_used;
        }
    }
    
    // 256 is the max number of possible palette entries (= 8pp),
    // only palette_entries will be valid.
    bmp_palette_element palette[256];
    for (int i = 0; i < palette_entries; i++) {
        palette[i].blue = read_uint8(reader);
        palette[i].green = read_uint8(reader);
        palette[i].red = read_uint8(reader);
        palette[i].reserved = read_uint8(reader);
    }
    
    // Fast forward to the bitmap data itself. We're usually already pointing at it,
    // but on the off chance this is v5 bitmap that has some embedded color profile
    // data, and they chose to stick it between the header and the bitmap data, well,
    // this should skip over it.
    buffered_reader_advance_to_offset(reader, file_header.bitmap_offset);
    
    // We are now pointing at the bitmap data itself. Walk the lines, and unpack
    // indexed colors as necessary.
    int width = header.width;
    int height = (header.height > 0) ? header.height : header.height * -1;
    int is_flipped = header.height < 0;

    // Figure out how many bytes in one "scan line" (stride) of the image data. Always
    // aligned to 4-byte boundaries.
    int bits_per_line = header.bits_per_pixel * width;
    if (bits_per_line % 32 != 0) {
        bits_per_line += (32 - bits_per_line % 32);
    }
    
    int bytes_per_line = bits_per_line / 8;

    // Final sanity check to make sure that enough bytes remain in the file to meet
    // our needs here.
    size_t raw_bitmap_size = bytes_per_line * height;
    if (!buffered_reader_ensure_remaining(reader, raw_bitmap_size)) {
        printf("Invalid BMP file\n");
        close_buffered_reader(reader);
        return NULL;
    }
    
    // Yank out the whole bitmap region.
    raw_bitmap_data = malloc(raw_bitmap_size);
    if (!raw_bitmap_data) {
        printf("Failed to allocate bitmap data\n");
        goto Error;

    }
    read_bytes(reader, raw_bitmap_data, raw_bitmap_size);
    
    bitmap = create_bitmap(width, height);
    if (!bitmap) {
        printf("Failed to allocate bitmap\n");
        goto Error;
    }

    // Loop through the bitmap. Note that this is looping through the *output* pixels,
    // and the bitmap file may not be "flipped" (ie, first line first)
    for (int y = 0; y < height; y++) {
        uint8_t * line_base = is_flipped ? (raw_bitmap_data + (y * bytes_per_line)) :
            (raw_bitmap_data + ((height - 1 - y) * bytes_per_line));
        uint8_t * next_pixel_start = line_base;
        int x = 0;
        while (x < width) {
            uint8_t byte = *next_pixel_start;
            uint8_t pixel_palette_indexes[8];          // only used for 1, 4, 8 bit
            uint8_t rgba[4];                           // only used for 24, 32 bit.
            switch (header.bits_per_pixel) {
                case 1:
                {
                    for (int p = 7; p >= 0; p--) {
                        pixel_palette_indexes[p] = (byte >> p) & 0x01;
                    }
                    next_pixel_start++;
                    break;
                }
                case 4:
                {
                    pixel_palette_indexes[0] = (byte >> 4) & 0x0F;
                    pixel_palette_indexes[1] = byte & 0x0F;
                    next_pixel_start++;
                    break;
                }
                case 8:
                {
                    pixel_palette_indexes[0] = byte;
                    next_pixel_start++;
                    break;
                }
                case 24:
                {
                    // Stored in BGR order
                    rgba[3] = 0xFF;
                    rgba[2] = *(next_pixel_start + 0);
                    rgba[1] = *(next_pixel_start + 1);
                    rgba[0] = *(next_pixel_start + 2);
                    next_pixel_start += 3;
                    break;
                }
                case 32:
                {
                    // We expect this to be in BGRA order, which is the default in the non-compressed
                    // format as well as the only "bitfields" ordering we support.
                    rgba[2] = *(next_pixel_start + 0);
                    rgba[1] = *(next_pixel_start + 1);
                    rgba[0] = *(next_pixel_start + 2);
                    rgba[3] = *(next_pixel_start + 3);
                    next_pixel_start += 4;
                    break;
                }
                default:
                    // Unreachable. We already validated the value set above.
                    break;
            }
            
            if (header.bits_per_pixel < 16) {
                // We have one or more palette indexes to indirect through.
                for (int i = 0; i < (8 / header.bits_per_pixel); i++) {
                    uint8_t index = pixel_palette_indexes[i];
                    if (index < palette_entries) {
                        int pixel_base = BITMAP_PIXEL_BASE(bitmap, x, y);
                        bitmap->rgba_pixels[pixel_base] = palette[index].red;
                        bitmap->rgba_pixels[pixel_base + 1] = palette[index].green;
                        bitmap->rgba_pixels[pixel_base + 2] = palette[index].blue;
                        bitmap->rgba_pixels[pixel_base + 3] = 0xFF;
                    }
                    // Advance the destination past this pixel and continue with this
                    // cluster.
                    x++;
                }
            } else {
                // We have computed the value of one pixel, write it.
                int pixel_base = BITMAP_PIXEL_BASE(bitmap, x, y);
                bitmap->rgba_pixels[pixel_base] = rgba[0];
                bitmap->rgba_pixels[pixel_base + 1] = rgba[1];
                bitmap->rgba_pixels[pixel_base + 2] = rgba[2];
                bitmap->rgba_pixels[pixel_base + 3] = rgba[3];
                x++;
            }
        }
    }
    
    goto Done;
    
Error:
    if (bitmap) {
        free_bitmap(bitmap);
    }
Done:
    if (raw_bitmap_data) {
        free(raw_bitmap_data);
    }
    close_buffered_reader(reader);
    return bitmap;
}
