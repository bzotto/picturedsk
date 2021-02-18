//
// woz_image.c
//
// Copyright (c) 2021 by Ben Zotto
//

#include "woz_image.h"

#define CHUNK_INITIAL_BUFFER 4096
#define WOZ_HEADER_SIZE      12
//
// Private routine declarations.
//
static void verify_writable_buffer(woz_chunk * chunk, size_t min);
static size_t serialize_chunk_to_buffer(woz_chunk * chunk, uint8_t * dest);

//
// Public routines.
//

woz_file * create_empty_woz_file(void)
{
    woz_file * woz = malloc(sizeof(woz_file));
    if (woz) {
        woz->info = create_woz_chunk("INFO");
        woz->tmap = create_woz_chunk("TMAP");
        woz->trks = create_woz_chunk("TRKS");
        woz->writ = create_woz_chunk("WRIT");
    }
    if (!woz || !woz->info || !woz->tmap || !woz->writ) {
        goto Error;
    }
    
    goto Done;
Error:
    if (woz) {
        free_woz_file(woz);
    }
    woz = NULL;
    
Done:
    return woz;
}

int write_woz_to_file(woz_file * woz, const char * path)
{
    FILE * file = fopen(path, "wb");
    if (!file) {
        printf("Failed to open output file %s", path);
        return -1;
    }
    
    // Calculate the total size needed to write each actual chunk.
    size_t total_file_size = WOZ_HEADER_SIZE;
    total_file_size += chunk_size_on_disk(woz->info);
    total_file_size += chunk_size_on_disk(woz->tmap);
    total_file_size += chunk_size_on_disk(woz->trks);
    total_file_size += chunk_size_on_disk(woz->writ);
    
    uint8_t * file_buffer = malloc(total_file_size);
    if (!file_buffer) {
        printf("Out of memory.\n");
        fclose(file);
        return -2;
    }
    
    file_buffer[0] = 'W';
    file_buffer[1] = 'O';
    file_buffer[2] = 'Z';
    file_buffer[3] = '2';
    file_buffer[4] = 0xFF;
    file_buffer[5] = '\n';
    file_buffer[6] = '\r';
    file_buffer[7] = '\n';
    // (CRC goes here at offset 8)
    size_t byte_index = WOZ_HEADER_SIZE;
    byte_index += serialize_chunk_to_buffer(woz->info, &file_buffer[byte_index]);
    byte_index += serialize_chunk_to_buffer(woz->tmap, &file_buffer[byte_index]);
    byte_index += serialize_chunk_to_buffer(woz->trks, &file_buffer[byte_index]);
    /* byte_index += */ serialize_chunk_to_buffer(woz->writ, &file_buffer[byte_index]);

    // Compute the overall CRC of everthing after the header, and write it in.
    uint32_t crc = woz_crc32(&file_buffer[WOZ_HEADER_SIZE], total_file_size - WOZ_HEADER_SIZE);
    file_buffer[8] = crc & 0xFF;
    file_buffer[9] = (crc >> 8) & 0xFF;
    file_buffer[10] = (crc >> 16) & 0xFF;
    file_buffer[11] = (crc >> 24) & 0xFF;

    // Write to disk.
    size_t bytes_written = fwrite(file_buffer, 1, total_file_size, file);

    fclose(file);
    free(file_buffer);

    if (bytes_written != total_file_size) {
        printf("Error writing woz output.\n");
        return -1;
    }
    
    return 0;
}

void free_woz_file(woz_file * woz)
{
    if (woz) {
        if (woz->info) { free_chunk(woz->info); }
        if (woz->tmap) { free_chunk(woz->tmap); }
        if (woz->trks) { free_chunk(woz->trks); }
        if (woz->writ) { free_chunk(woz->writ); }
        free(woz);
    }
}

woz_chunk * create_woz_chunk(const char * name)
{
    woz_chunk * chunk = malloc(sizeof(woz_chunk));
    if (!chunk) {
        return NULL;
    }
    chunk->data = calloc(CHUNK_INITIAL_BUFFER, 1);
    if (!chunk->data) {
        free(chunk);
        return NULL;
    }
    memcpy(chunk->name, name, 4);
    chunk->buffer_size = CHUNK_INITIAL_BUFFER;
    chunk->mark = 0;
    return chunk;
}

void free_chunk(woz_chunk * chunk)
{
    free(chunk->data);
    free(chunk);
}

size_t chunk_size_on_disk(woz_chunk * chunk)
{
    // 4 bytes for the name characters, 4 for the length value.
    return 4 + 4 + chunk->mark;
}

void chunk_write_uint8(woz_chunk * chunk, uint8_t u8)
{
    verify_writable_buffer(chunk, sizeof(uint8_t));
    chunk->data[chunk->mark++] = u8;
}

void chunk_write_uint16(woz_chunk * chunk, uint16_t u16)
{
    verify_writable_buffer(chunk, sizeof(uint16_t));
    chunk->data[chunk->mark++] = u16 & 0x00FF;
    chunk->data[chunk->mark++] = (u16 >> 8) & 0x00FF;
}

void chunk_write_uint32(woz_chunk * chunk, uint32_t u32)
{
    verify_writable_buffer(chunk, sizeof(uint32_t));
    chunk->data[chunk->mark++] = u32 & 0x000000FF;
    chunk->data[chunk->mark++] = (u32 >> 8) & 0x000000FF;
    chunk->data[chunk->mark++] = (u32 >> 16) & 0x000000FF;
    chunk->data[chunk->mark++] = (u32 >> 24) & 0x000000FF;
}

// This routine expects utf8string to be both a valid UTF string and
// NUL terminated. If the string is longer than n character, it will be
// truncated. The resulting string will not be NUL terminated but will be
// padded with space characters.
void chunk_write_utf8(woz_chunk * chunk, const char * utf8string, int n)
{
    verify_writable_buffer(chunk, n);
    size_t len = strlen(utf8string);
    uint8_t * dest = &chunk->data[chunk->mark];
    for (int i = 0; i < n; i++) {
        dest[i] = (i < len) ? utf8string[i] : 0x20;
    }
    chunk->mark += n;
}

void chunk_write_bytes(woz_chunk * chunk, const uint8_t * bytes, size_t n)
{
    verify_writable_buffer(chunk, n);
    memcpy(&chunk->data[chunk->mark], bytes, n);
    chunk->mark += n;
}

void chunk_set_mark(woz_chunk * chunk, size_t mark)
{
    if (mark <= chunk->mark) {
        chunk->mark = mark;
        return;
    }
    verify_writable_buffer(chunk, mark - chunk->mark);
    chunk->mark = mark;
}

void chunk_advance_mark(woz_chunk * chunk, int offset)
{
    size_t new_mark = chunk->mark + offset;
    chunk_set_mark(chunk, new_mark);
}

//
// Private helper routines.
//

static
void verify_writable_buffer(woz_chunk * chunk, size_t min)
{
    if (chunk->mark + min > chunk->buffer_size) {
        size_t new_size = chunk->buffer_size * 2;
        if (chunk->mark + min > new_size) {
            new_size += min;
        }
        uint8_t * new_buffer = calloc(new_size, 1);
        if (!new_buffer) {
            // We don't handle this error which should never happen...
            // Will crash on the memcpy that comes next...
            printf("Out of memory expanding chunk buffer.");
        }
        memcpy(new_buffer, chunk->data, chunk->buffer_size);
        free(chunk->data);
        chunk->data = new_buffer;
        chunk->buffer_size = new_size;
    }
}

static
size_t serialize_chunk_to_buffer(woz_chunk * chunk, uint8_t * dest)
{
    memcpy(dest, &chunk->name, 4);
    size_t size = chunk->mark;
    dest[4] = size & 0xFF;
    dest[5] = size >> 8 & 0xFF;
    dest[6] = size >> 16 & 0xFF;
    dest[7] = size >> 24 & 0xFF;
    memcpy(&dest[8], chunk->data, chunk->mark);
    return 4 + 4 + chunk->mark;
}


//
// CRC routine and table.
// Gary S. Brown, 1986.
// Copied from https://applesaucefdc.com/woz/reference2/
//

static uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t woz_crc32(const void *buf, size_t size)
{
    const uint8_t * p = buf;
    uint32_t crc = 0 ^ ~0U;
    while (size--) {
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ ~0U;
}
