/**
 * TinyBits Amalgamated Header
 * Generated on: Sun May  4 05:43:30 PM CEST 2025
 */

#ifndef TINY_BITS_H
#define TINY_BITS_H

/* Begin common.h */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> // for size_t
#include <math.h>
#include <stdio.h>

#define TB_HASH_SIZE 128
#define TB_HASH_CACHE_SIZE 256
#define MAX_BYTES 9
#define TB_DDP_STR_LEN_MAX 128

// main tags
#define TB_INT_TAG 0x80     // +/- integer
#define TB_REF_TAG 0x60     // deduped string
#define TB_STR_TAG 0x40     // string
#define TB_DBL_TAG 0x20     // double value
#define TB_PFP_TAG 0x20     // + compressed double
#define TB_NFP_TAG 0x30     // - compressed double
#define TB_NAN_TAG 0x2D     // NaN
#define TB_INF_TAG 0x3D     // Infinity
#define TB_NNF_TAG 0x2E     // -Infinity
#define TB_F16_TAG 0x3E     // f16 
#define TB_F32_TAG 0x2F     // float (32bit)
#define TB_F64_TAG 0x3F     // double (64bit)
#define TB_MAP_TAG 0x10     // map { key: value}
#define TB_ARR_TAG 0x08     // array [element1, element2]
#define TB_DTM_TAG 0x07     // datetime
#define TB_NXT_TAG 0x06     // native extensions (multibyte tags)
#define TB_SEP_TAG 0x05     // separator (for group deduplication)
#define TB_EXT_TAG 0x04     // extension (user extentions)
#define TB_BLB_TAG 0x03     // blob
#define TB_NIL_TAG 0x02     // NULL
#define TB_TRU_TAG 0x01     // TRUE
#define TB_FLS_TAG 0x00     // FALSE

// Length values (for string, map & array)
#define TB_STR_LEN 0x1F     // max embedded string length
#define TB_REF_LEN 0x1F     // max embedded reference id
#define TB_MAP_LEN 0x0F     // max embedded map length
#define TB_ARR_LEN 0x07     // max embedded array length

// native extensions TR_NXT_TAG

// Feature flags (from encoder)
#define TB_FEATURE_STRING_DEDUPE    0x01
#define TB_FEATURE_COMPRESS_FLOATS  0x02

static double powers[] = {
    1.0, 
    10.0, 
    100.0, 
    1000.0, 
    10000.0, 
    100000.0, 
    1000000.0, 
    10000000.0, 
    100000000.0, 
    1000000000.0, 
    10000000000.0, 
    100000000000.0, 
    1000000000000.0
};

typedef struct HashEntry {
    uint32_t hash;          // 32-bit hash from fast_hash_32
    uint32_t length;
    uint32_t offset;
    uint32_t next_index;
} HashEntry;

typedef struct HashTable {
    HashEntry* cache; // HASH_SIZE is 2048, use directly or define HASH_SIZE in header
    uint32_t next_id;
    uint32_t cache_size;
    uint32_t cache_pos;
    uint8_t bins[TB_HASH_SIZE];
} HashTable;

static inline uint32_t fast_hash_32(const char* str, uint16_t len) {
    uint32_t hash = len;
    hash = (hash << 24) | 
           ((unsigned char)str[0] << 16) | 
           ((unsigned char)str[1] << 8 ) | 
           ((unsigned char)str[len-1]);
    //hash ^= (((unsigned char)str[len-2] << 24) | ((unsigned char)str[len-1] << 16));
    return hash;
}

static inline int encode_varint(uint64_t value, uint8_t* buffer) {
    if (value <= 240) {
        buffer[0] = (uint8_t)value;  // 1 byte
        return 1;
    } else if (value < 2288) {  // 241 to 248
        value -= 240;
        int prefix = 241 + (value / 256);
        buffer[0] = (uint8_t)prefix;  // A0
        buffer[1] = (uint8_t)(value % 256);  // A1
        return 2;
    } else if (value <= 67823) {  // Up to 249
        value -= 2288;
        buffer[0] = 249;  // A0
        buffer[1] = (uint8_t)(value / 256);  // A1
        buffer[2] = (uint8_t)(value % 256);  // A2
        return 3;
    } else if (value < (1ULL << 24)) {  // 250: 3-byte big-endian
        buffer[0] = 250;  // A0
        buffer[1] = (uint8_t)(value >> 16);  // A1 (most significant)
        buffer[2] = (uint8_t)(value >> 8);   // A2
        buffer[3] = (uint8_t)value;          // A3 (least significant)
        return 4;
    } else if (value < (1ULL << 32)) {  // 251: 4-byte big-endian
        buffer[0] = 251;  // A0
        buffer[1] = (uint8_t)(value >> 24);
        buffer[2] = (uint8_t)(value >> 16);
        buffer[3] = (uint8_t)(value >> 8);
        buffer[4] = (uint8_t)value;
        return 5;
    } else if (value < (1ULL << 40)) {  // 252: 5-byte big-endian
        buffer[0] = 252;  // A0
        buffer[1] = (uint8_t)(value >> 32);
        buffer[2] = (uint8_t)(value >> 24);
        buffer[3] = (uint8_t)(value >> 16);
        buffer[4] = (uint8_t)(value >> 8);
        buffer[5] = (uint8_t)value;
        return 6;
    } else if (value < (1ULL << 48)) {  // 253: 6-byte big-endian
        buffer[0] = 253;  // A0
        buffer[1] = (uint8_t)(value >> 40);
        buffer[2] = (uint8_t)(value >> 32);
        buffer[3] = (uint8_t)(value >> 24);
        buffer[4] = (uint8_t)(value >> 16);
        buffer[5] = (uint8_t)(value >> 8);
        buffer[6] = (uint8_t)value;
        return 7;
    } else if (value < (1ULL << 56)) {  // 254: 7-byte big-endian
        buffer[0] = 254;  // A0
        buffer[1] = (uint8_t)(value >> 48);
        buffer[2] = (uint8_t)(value >> 40);
        buffer[3] = (uint8_t)(value >> 32);
        buffer[4] = (uint8_t)(value >> 24);
        buffer[5] = (uint8_t)(value >> 16);
        buffer[6] = (uint8_t)(value >> 8);
        buffer[7] = (uint8_t)value;
        return 8;
    } else {  // 255: 8-byte big-endian
        buffer[0] = 255;  // A0
        buffer[1] = (uint8_t)(value >> 56);
        buffer[2] = (uint8_t)(value >> 48);
        buffer[3] = (uint8_t)(value >> 40);
        buffer[4] = (uint8_t)(value >> 32);
        buffer[5] = (uint8_t)(value >> 24);
        buffer[6] = (uint8_t)(value >> 16);
        buffer[7] = (uint8_t)(value >> 8);
        buffer[8] = (uint8_t)value;
        return 9;
    }
}

static inline int varint_size(uint64_t value){
    if (value < (1ULL << 48)) {  // 253: 6-byte big-endian
        return 7;
    } else if(value  < (1ULL << 52)){
        return 8;
    }
    return 9;
}

static inline uint64_t decode_varint(const uint8_t* buffer, size_t size, size_t *pos) {
    uint8_t prefix = buffer[*pos];
    if (prefix <= 240) {
        *pos += 1;
        return prefix;
    } else if (prefix >= 241 && prefix <= 248) {
        uint64_t value = 240 + 256 * (prefix - 241) + buffer[*pos+1];
        *pos += 2;
        return value;
    } else if (prefix == 249) {
        uint64_t value = 2288 + 256 * buffer[*pos+1] + buffer[*pos+2];
        *pos += 3;
        return value;
    } else if (prefix == 250) {
        uint64_t value = ((uint64_t)buffer[*pos+1] << 16) | ((uint64_t)buffer[*pos+2] << 8) | buffer[*pos+3];
        *pos += 4;
        return value;
    } else if (prefix == 251) {
        uint64_t value = ((uint64_t)buffer[*pos+1] << 24) | ((uint64_t)buffer[*pos+2] << 16) |
               ((uint64_t)buffer[*pos+3] << 8) | buffer[*pos+4];
        *pos += 5;
        return value;
    } else if (prefix == 252) {
        uint64_t value = ((uint64_t)buffer[*pos+1] << 32) | ((uint64_t)buffer[*pos+2] << 24) |
               ((uint64_t)buffer[*pos+3] << 16) | ((uint64_t)buffer[*pos+4] << 8) | buffer[*pos+5];
        *pos += 6;
        return value;
    } else if (prefix == 253) {
        uint64_t value = ((uint64_t)buffer[*pos+1] << 40) | ((uint64_t)buffer[*pos+2] << 32) |
               ((uint64_t)buffer[*pos+3] << 24) | ((uint64_t)buffer[*pos+4] << 16) |
               ((uint64_t)buffer[*pos+5] << 8) | buffer[*pos+6];
        *pos += 7;
        return value;       
    } else if (prefix == 254) {
        uint64_t value = ((uint64_t)buffer[*pos+1] << 48) | ((uint64_t)buffer[*pos+2] << 40) |
               ((uint64_t)buffer[*pos+3] << 32) | ((uint64_t)buffer[*pos+4] << 24) |
               ((uint64_t)buffer[*pos+5] << 16) | ((uint64_t)buffer[*pos+6] << 8) | buffer[*pos+7];
        *pos += 8;
        return value;       
    } else if (prefix == 255) {
        uint64_t value = ((uint64_t)buffer[*pos+1] << 56) | ((uint64_t)buffer[*pos+2] << 48) |
               ((uint64_t)buffer[*pos+3] << 40) | ((uint64_t)buffer[*pos+4] << 32) |
               ((uint64_t)buffer[*pos+5] << 24) | ((uint64_t)buffer[*pos+6] << 16) |
               ((uint64_t)buffer[*pos+7] << 8) | buffer[*pos+8];
        *pos += 9;
        return value;
    } else {
        return 0;  // Error case
    }
}

static inline int fast_memcmp(const void *ptr1, const void *ptr2, size_t num) {
    if(num < 32){
        const unsigned char *p1 = (const unsigned char*)ptr1;
        const unsigned char *p2 = (const unsigned char*)ptr2;
        for(size_t i = 0; i < num; i++){
            if(p1[i] != p2[i]) return 1; 
        }
    }else{
        return memcmp(ptr1, ptr2, num); 
    }
    return 0;
}

static inline void *fast_memcpy(unsigned char *ptr1, const char *ptr2, size_t num) {
    for(size_t i = 0; i < num; i++){
        ptr1[i] = ptr2[i]; 
    }
    return ptr1;
}

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

static inline uint64_t dtoi_bits(double d) {
    union {
        double d;
        uint64_t u;
    } converter;    
    converter.d = d;
    return converter.u;
}

static inline double itod_bits(uint64_t u) {
    union {
        double d;
        uint64_t u;
    } converter;
    converter.u = u;
    return converter.d;
}

static inline void encode_uint64( uint64_t value, uint8_t *buffer) {
    buffer[0] = (value >> 56) & 0xFF;
    buffer[1] = (value >> 48) & 0xFF;
    buffer[2] = (value >> 40) & 0xFF;
    buffer[3] = (value >> 32) & 0xFF;
    buffer[4] = (value >> 24) & 0xFF;
    buffer[5] = (value >> 16) & 0xFF;
    buffer[6] = (value >> 8) & 0xFF;
    buffer[7] = value & 0xFF;
}

static inline uint64_t decode_uint64(const uint8_t *buffer) {
    return ((uint64_t)buffer[0] << 56) |
           ((uint64_t)buffer[1] << 48) |
           ((uint64_t)buffer[2] << 40) |
           ((uint64_t)buffer[3] << 32) |
           ((uint64_t)buffer[4] << 24) |
           ((uint64_t)buffer[5] << 16) |
           ((uint64_t)buffer[6] << 8) |
            (uint64_t)buffer[7];
}

static inline int decimal_places_count(double abs_val, double *scaled) {
    //double abs_val = fabs(val);
    *scaled = abs_val;
    double temp = *scaled;
    if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 0;}

    *scaled = abs_val * 10000;
    temp = *scaled;
    if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { 
        *scaled = abs_val * 10;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 1;}
        *scaled = abs_val * 100;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 2;}
        *scaled = abs_val * 1000;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 3;}
        *scaled = temp;
        return 4;
    }

    *scaled = abs_val * 100000000;
    temp = *scaled;
    if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { 
        *scaled = abs_val * 100000;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 5;}
        *scaled = abs_val * 1000000;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 6;}
        *scaled = abs_val * 10000000;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 7;}
        *scaled = temp;
        return 8;
    }

    *scaled = abs_val * 1000000000000;
    temp = *scaled;
    if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { 
        *scaled = abs_val * 1000000000;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 9;}
        *scaled = abs_val * 10000000000;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 10;}
        *scaled = abs_val * 100000000000;
        if(*scaled == (uint64_t)(*scaled) && *scaled >= abs_val) { return 11;}
        *scaled = temp;
        return 12;
    }
    return -1;
}

/* End common.h */

/* Begin packer.h */


typedef struct tiny_bits_packer {
    unsigned char *buffer;      // Pointer to the allocated buffer
    size_t capacity;         // Total allocated size of the buffer
    size_t current_pos;      // Current position in the buffer (write position)
    HashTable encode_table; // Add the hash table here
    HashTable dictionary;
    uint8_t features;
    // Add any other encoder-specific state here if needed (e.g., string deduplication table later)
} tiny_bits_packer;

static inline unsigned char *tiny_bits_packer_ensure_capacity(tiny_bits_packer *encoder, size_t needed_size) {
    if (!encoder) return NULL;

    size_t available_space = encoder->capacity - encoder->current_pos;
    if (needed_size > available_space) {
        size_t new_capacity = encoder->capacity + needed_size + (encoder->capacity);
        unsigned char *new_buffer = (unsigned char *)realloc(encoder->buffer, new_capacity);
        if (!new_buffer) return NULL;
        encoder->buffer = new_buffer;
        encoder->capacity = new_capacity;
    }
    return encoder->buffer + encoder->current_pos;
}

/**
 * @brief allocates and initializes a new packer
 * 
 * @return pointer to new packer instance
 * 
 * @note the returned packer object must be freed using tiny_bits_packer_destroy()
 */
tiny_bits_packer *tiny_bits_packer_create(size_t initial_capacity, uint8_t features) {
    tiny_bits_packer *encoder = (tiny_bits_packer *)malloc(sizeof(tiny_bits_packer));
    if (!encoder) return NULL;

    encoder->buffer = (unsigned char *)malloc(initial_capacity);
    if (!encoder->buffer) {
        free(encoder);
        return NULL;
    }
    encoder->capacity = initial_capacity;
    encoder->current_pos = 0;
    encoder->features = features;

    // Only allocate hash table if deduplication is enabled
    if (features & TB_FEATURE_STRING_DEDUPE) {
        encoder->encode_table.cache = (HashEntry*)malloc(sizeof(HashEntry) * TB_HASH_CACHE_SIZE);
        if (!encoder->encode_table.cache) {
            //free(encoder->encode_table.buckets);
            free(encoder->buffer);
            free(encoder);
            return NULL;
        }
        encoder->encode_table.cache_size = TB_HASH_CACHE_SIZE;
        encoder->encode_table.cache_pos = 0;
        encoder->encode_table.next_id = 0;
    } else {
        encoder->encode_table.cache = NULL;
        encoder->encode_table.cache_size = 0;
        encoder->encode_table.cache_pos = 0;
        encoder->encode_table.next_id = 0;
    }

    return encoder;
}

/**
 * @brief Resets internal data structure of the packer object
 * 
 * @param encoder The packer instance
 *
 * @note This function allows for more efficient packing by reusing the same packer object
 */
inline void tiny_bits_packer_reset(tiny_bits_packer *encoder) {
    if (!encoder) return;
    encoder->current_pos = 0;  
    if (encoder->features & TB_FEATURE_STRING_DEDUPE) {
        encoder->encode_table.next_id = 0;
        encoder->encode_table.cache_pos = 0;
        memset(encoder->encode_table.bins, 0, TB_HASH_SIZE * sizeof(uint8_t));
    }
    
}

/**
 * @brief Deallocate the packer object and its internal data structures
 * 
 * @param encoder The unpacker instance
 *
 */
void tiny_bits_packer_destroy(tiny_bits_packer *encoder) {
    if (!encoder) return;
    
    if (encoder->features & TB_FEATURE_STRING_DEDUPE) {
        free(encoder->encode_table.cache);
    }
    free(encoder->buffer);
    free(encoder);
}

/**
 * @brief Packs an array header into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @param arr_len Number of elements in the array
 * @return Number of bytes written, or 0 on error
 * 
 * @note This function only writes the array header, not the elements themselves
 */
static inline int pack_arr(tiny_bits_packer *encoder, int arr_len){
    int written = 0;
    int needed_size;
    uint8_t *buffer;

    if(arr_len < TB_ARR_LEN){
      needed_size = 1;
    } else {
      needed_size = 1 + varint_size((uint64_t)(arr_len - 7));
    }
    buffer = tiny_bits_packer_ensure_capacity(encoder, needed_size);
    if (!buffer) return 0; // Handle error

    if(arr_len < TB_ARR_LEN){
      buffer[0] = TB_ARR_TAG | arr_len;
      written = 1;
    } else {
      buffer[0] = TB_ARR_TAG | TB_ARR_LEN;
      written = 1;
      written += encode_varint((uint64_t)(arr_len - TB_ARR_LEN), buffer + written);
    }
    encoder->current_pos += written;
    return written;
}

/**
 * @brief Packs a map header into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @param map_len Number of key-value pairs in the map
 * @return Number of bytes written, or 0 on error
 * 
 * @note This function only writes the map header, not the key-value pairs themselves
 */
static inline int pack_map(tiny_bits_packer *encoder, int map_len){
    int written = 0;
    int needed_size;
    uint8_t *buffer;

    if(map_len < TB_MAP_LEN){
      needed_size = 1;
    } else {
      needed_size = 1 + varint_size((uint64_t)(map_len - 15));
    }
    buffer = tiny_bits_packer_ensure_capacity(encoder, needed_size);
    if (!buffer) return 0; // Handle error

    if(map_len < TB_MAP_LEN){
      buffer[0] = TB_MAP_TAG | map_len;
      written = 1;
    } else {
      buffer[0] = TB_MAP_TAG | TB_MAP_LEN;
      written = 1;
      written += encode_varint((uint64_t)(map_len - TB_MAP_LEN), buffer + written);
    }
    encoder->current_pos += written;
    return written;
}

/**
 * @brief Packs an integer value into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @param value The integer value to pack
 * @return Number of bytes written, or 0 on error
 * 
 * @note Uses a compact representation for small values and SQLite4 like integer encoding for larger values
 */
static inline int pack_int(tiny_bits_packer *encoder, int64_t value){
    int written = 0;
    int needed_size = 10;
    uint8_t *buffer;
    buffer = tiny_bits_packer_ensure_capacity(encoder, needed_size);
    if (!buffer) return 0; // Handle error
    //printf("value is %ld\n", value);

    if (value >= 0 && value < 120) {
        buffer[0] = (uint8_t)(TB_INT_TAG | value);  // No continuation
        //printf("value is %ld, wrote to buffer %x\n", value, buffer[0]);
        encoder->current_pos += 1;
        return 1;
    } else if (value >= 120) {
        buffer[0] = 248;  // Tag for positive with continuation
        value -= 120;
    } else if (value > -7) {
        buffer[0] = (uint8_t)(248 + (-value));  // No continuation
        encoder->current_pos += 1;
        return 1;
    } else {
        buffer[0] = 255;  // Tag for negative with continuation
        value = -(value + 7);  // Store positive magnitude
    }
    // Encode continuation bytes in BER format (7 bits per byte)
    written += encode_varint(value, buffer + 1) + 1 ;
    encoder->current_pos += written;
    return written;
}

static inline int _pack_tag_only(tiny_bits_packer *encoder, uint8_t tag){
    uint8_t *buffer = tiny_bits_packer_ensure_capacity(encoder, 1);
    if (!buffer) return 0; // Handle error
    buffer[0] = tag;
    encoder->current_pos += 1;
    return 1;

}

/**
 * @brief Packs a separator tag into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @return Number of bytes written, or 0 on error
 */
static inline int pack_separator(tiny_bits_packer *encoder){
    return _pack_tag_only(encoder, (uint8_t)TB_SEP_TAG);
}

/**
 * @brief Packs a NULL value into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @return Number of bytes written, or 0 on error
 */
static inline int pack_null(tiny_bits_packer *encoder){
    return _pack_tag_only(encoder, (uint8_t)TB_NIL_TAG);
}

/**
 * @brief Packs a TRUE boolean value into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @return Number of bytes written, or 0 on error
 */
static inline int pack_true(tiny_bits_packer *encoder){
    return _pack_tag_only(encoder, (uint8_t)TB_TRU_TAG);
}

/**
 * @brief Packs a FALSE boolean value into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @return Number of bytes written, or 0 on error
 */
static inline int pack_false(tiny_bits_packer *encoder){
    return _pack_tag_only(encoder, (uint8_t)TB_FLS_TAG);
}

/**
 * @brief Packs a NaN (Not a Number) value into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @return Number of bytes written, or 0 on error
 */
static inline int pack_nan(tiny_bits_packer *encoder){
    return _pack_tag_only(encoder, (uint8_t)TB_NAN_TAG);
}

/**
 * @brief Packs a positive infinity value into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @return Number of bytes written, or 0 on error
 */
static inline int pack_infinity(tiny_bits_packer *encoder){
    return _pack_tag_only(encoder, (uint8_t)TB_INF_TAG);
}

/**
 * @brief Packs a negative infinity value into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @return Number of bytes written, or 0 on error
 */
static inline int pack_negative_infinity(tiny_bits_packer *encoder){
    return _pack_tag_only(encoder, (uint8_t)TB_NNF_TAG);
}

/**
 * @brief Packs a string into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @param str Pointer to the string data
 * @param str_len Length of the string in bytes
 * @return Number of bytes written, or 0 on error
 * 
 * @note If string deduplication is enabled, this may store a reference to a previously stored string
 */
static inline int pack_str(tiny_bits_packer *encoder, char* str, uint32_t str_len) {
    uint32_t id = 0;
    int found = 0;
    int written = 0;
    int needed_size = 0;
    uint8_t *buffer;
    uint32_t hash_code = 0;
    uint32_t hash = 0;
    if ((encoder->features & TB_FEATURE_STRING_DEDUPE) && str_len >= 2 && str_len <= 128) {
        hash_code = fast_hash_32(str, str_len);
        hash = hash_code % TB_HASH_SIZE;
        uint8_t index = encoder->encode_table.bins[hash];
        while (index > 0) {
            HashEntry entry = encoder->encode_table.cache[index - 1];
            if (hash_code == entry.hash 
                && str_len == entry.length
                && fast_memcmp(str, encoder->buffer + entry.offset, str_len) == 0 ) {
                id = index - 1;
                found = 1;
                break;
            }
            index = entry.next_index;
        }
    }

    if (found) {
        // Encode existing string ID
        if (id < 31) {
            needed_size = 1;
        } else {
            needed_size = 1 + varint_size(id - 31);
        }
        buffer = tiny_bits_packer_ensure_capacity(encoder, needed_size);
        if (!buffer) return 0;

        if (id < TB_REF_LEN) {
            buffer[0] = TB_REF_TAG | id;
            written = 1;
        } else {
            buffer[0] = TB_REF_TAG | TB_REF_LEN;
            written = 1;
            written += encode_varint(id - TB_REF_LEN, buffer + written);
        }
    } else {
       needed_size = 10 + str_len;
        buffer = tiny_bits_packer_ensure_capacity(encoder, needed_size);
        if (!buffer) return 0;

        if (str_len < TB_STR_LEN) {
            buffer[0] = TB_STR_TAG | str_len;
            written = 1;
            fast_memcpy(buffer + written, str, str_len);
            written += str_len;
        } else {
            buffer[0] = TB_STR_TAG | TB_STR_LEN;
            written = 1;
            written += encode_varint(str_len - TB_STR_LEN, buffer + written);
            memcpy(buffer + written, str, str_len);
            written += str_len;
        }
        
        if ((encoder->features & TB_FEATURE_STRING_DEDUPE) 
            && encoder->encode_table.cache_pos < TB_HASH_CACHE_SIZE
            && str_len >= 2 && str_len <= 128){ 
            HashEntry* new_entry = &encoder->encode_table.cache[encoder->encode_table.cache_pos++];
            new_entry->hash = hash_code; 
            new_entry->length = str_len;
            new_entry->offset = encoder->current_pos + written - str_len;
            new_entry->next_index = encoder->encode_table.bins[hash];
            encoder->encode_table.bins[hash] = encoder->encode_table.cache_pos;
        }

    }

    encoder->current_pos += written;
    return written;
}

/**
 * @brief Packs a double-precision floating point value into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @param val The double value to pack
 * @return Number of bytes written, or 0 on error
 * 
 * @note If TB_FEATURE_COMPRESS_FLOATS is enabled, this will use a more compact representation for some values
 */
static inline int pack_double(tiny_bits_packer *encoder, double val) {
    int written = 0;
    uint8_t *buffer = tiny_bits_packer_ensure_capacity(encoder, 10);
    if (!buffer) return 0;
    if(isnan(val)){
      return pack_nan(encoder);
    } 
    if(isinf(val)){
      if(val > 0){
        return pack_infinity(encoder);
      } else {
        return pack_negative_infinity(encoder);
      }
    }
    // scaled varint encoding
    if (encoder->features & TB_FEATURE_COMPRESS_FLOATS) {
        double abs_val = fabs(val); ///val >= 0 ? val : -val;
        double scaled; //= abs_val;
        int multiplies = decimal_places_count(abs_val, &scaled);
        if(multiplies >= 0){
            uint64_t integer = (uint64_t)scaled;
            if(integer < (1ULL << 48)) {
                if (!buffer) return 0;
                if(val >= 0){
                    buffer[0] = TB_PFP_TAG | (multiplies);
                } else {
                    buffer[0] = TB_NFP_TAG | (multiplies);
                }
                written++;
                written += encode_varint(integer, buffer + written);
                encoder->current_pos += written;
                return written;
            }
        }

    }
    // Fallback to raw double
    buffer[0] = TB_F64_TAG;
    written++;
    encode_uint64(dtoi_bits(val), buffer + written);
    written += 8;
    encoder->current_pos += written;
    return written;
}

/**
 * @brief Packs a unixtime double-precision floating point value, along with a time zone offset into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @param val The unixtime double value to pack
 * @param offset The timezone offset (as a +/- seconds)
 * @return Number of bytes written, or 0 on error
 * 
 */
static inline int pack_datetime(tiny_bits_packer *encoder, double val, int16_t offset) {
    int written = 0;
    uint8_t *buffer = tiny_bits_packer_ensure_capacity(encoder, 11);
    if (!buffer) return 0;
    buffer[0] = TB_DTM_TAG;
    buffer[1] = (int8_t) ((offset % 86400) / (60*15)); // convert seconds to multiples of 15 minutes
    written += 2;
    encode_uint64(dtoi_bits(val), buffer + written);
    written += 8;
    encoder->current_pos += written;
    //written += pack_double(encoder, val);
    return written;
}

/**
 * @brief Packs a binary blob (byte array) into the buffer
 * 
 * @param encoder Pointer to the packer instance
 * @param blob Pointer to the binary data
 * @param blob_size Size of the binary data in bytes
 * @return Number of bytes written, or 0 on error
 */
static inline int pack_blob(tiny_bits_packer *encoder, const char* blob, int blob_size){
    int written = 0;
    int needed_size;
    uint8_t *buffer;

    needed_size = 1 + varint_size((uint64_t)blob_size) + blob_size;
    buffer = tiny_bits_packer_ensure_capacity(encoder, needed_size);
    if (!buffer) return 0; // Handle error

    buffer[0] = (uint8_t)TB_BLB_TAG;
    written++;
    written += encode_varint((uint64_t)blob_size, buffer + written);
    memcpy(buffer + written, blob, blob_size);
    written += blob_size;
    encoder->current_pos += written;
    return written;
}

/* End packer.h */

/* Begin unpacker.h */



// Decoder return types
enum tiny_bits_type {
    TINY_BITS_ARRAY,    // length: number of elements
    TINY_BITS_MAP,      // length: number of key-value pairs
    TINY_BITS_INT,      // int_val: integer value
    TINY_BITS_DOUBLE,   // double_val: double value
    TINY_BITS_STR,      // str_blob_val.length: byte length of string, str_blob_val.data: pointer to string
    TINY_BITS_BLOB,     // str_blob_val.length: byte length of blob, str_blob_val.data: pointer to blob
    TINY_BITS_TRUE,     // No value
    TINY_BITS_FALSE,    // No value
    TINY_BITS_NULL,     // No value
    TINY_BITS_NAN,      // No value
    TINY_BITS_INF,      // No value
    TINY_BITS_N_INF,    // No value
    TINY_BITS_EXT,      // No value
    TINY_BITS_SEP,      // No balue
    TINY_BITS_FINISHED, // End of buffer
    TINY_BITS_ERROR,     // Parsing error
    TINY_BITS_DATETIME   // double_val: double value
};

// value union
typedef union tiny_bits_value {
    int64_t int_val;    // TINY_BITS_INT
    double double_val;  // TINY_BITS_DOUBLE
    size_t length;      // TINY_BITS_ARRAY, TINY_BITS_MAP,
    struct {            // TINY_BITS_STR, TINY_BITS_BLOB
        const char *data; 
        size_t length;
        int32_t id;
    } str_blob_val;
    struct {            // TINY_BITS_STR, TINY_BITS_BLOB
        double unixtime;
        size_t offset;
    } datetime_val;   
} tiny_bits_value;

// The unpacker data structure
typedef struct tiny_bits_unpacker {
    const unsigned char *buffer;  // Input buffer (read-only)
    size_t size;                  // Total size of buffer
    size_t current_pos;           // Current read position
    struct {
        char *str;    // Pointer to decompressed string data (owned by strings array)
        size_t length; // Length of string
    } *strings;           // Array of decoded strings
    size_t strings_size;  // Capacity of strings array
    size_t strings_count; // Number of strings stored
    HashTable dictionary;
} tiny_bits_unpacker;

/**
 * @brief allocates and initializes a new unpacker
 * 
 * @return pointer to new unpacker instance
 * 
 * @note the returned unpacker object must be freed using tiny_bits_unpacker_destroy()
 */
tiny_bits_unpacker *tiny_bits_unpacker_create(void) {

    tiny_bits_unpacker *decoder = (tiny_bits_unpacker *)malloc(sizeof(tiny_bits_unpacker));
    if (!decoder) return NULL;
    // String array setup
    decoder->strings_size = 8; // Initial capacity
    decoder->strings = (void *)malloc(decoder->strings_size * sizeof(*decoder->strings));
    if (!decoder->strings) {
        free(decoder);
        return NULL;
    }
    decoder->strings_count = 0;
    return decoder;
}

/**
 * @breif Provides a buffer to the unpacker for unpacking
 * 
 * @param decoder The unpakcer instance
 *
 * @param buffer A pointer to the buffer
 *
 * @param size Size of the region to be unpacked
 *
 * @note This function implicitly resets the unpacker object so no need to call tiny_bits_unpacker_reset()
 */
static inline void tiny_bits_unpacker_set_buffer(tiny_bits_unpacker *decoder, const unsigned char *buffer, size_t size) {
    if (!decoder) return;
    if (!buffer || size < 1) return;
    decoder->buffer = buffer;
    decoder->size = size;
    decoder->current_pos = 0;
    decoder->strings_count = 0;
}

/**
 * @brief Resets internal data structure of the unpacker object
 * 
 * @param decoder The unpacker instance
 *
 * @note This function is useful if you want to operate on the same buffer again for some reason
 */
static inline void tiny_bits_unpacker_reset(tiny_bits_unpacker *decoder) {
    if (!decoder) return;
    decoder->current_pos = 0;
    decoder->strings_count = 0;
}


/**
 * @brief Deallocate the unpacker object and its internal data structures
 * 
 * @param decoder The unpacker instance
 *
 */
void tiny_bits_unpacker_destroy(tiny_bits_unpacker *decoder) {
    if (!decoder) return;
    if (decoder->strings) {
        free(decoder->strings);
    }
    free(decoder);
}

static inline enum tiny_bits_type _unpack_int(tiny_bits_unpacker *decoder, uint8_t tag, tiny_bits_value *value){
        size_t pos = decoder->current_pos;
        if (tag < 248) { // Small positive (128-247)
            value->int_val = tag - 128;
            return TINY_BITS_INT;
        } else if (tag == 248) { // Positive with continuation
            uint64_t val = decode_varint(decoder->buffer, decoder->size, &pos);
            value->int_val = val + 120;
            decoder->current_pos = pos;
            return TINY_BITS_INT;
        } else if (tag > 248 && tag < 255) { // Small negative (248-254)
            value->int_val = -(tag - 248);
            return TINY_BITS_INT;
        } else { // 255: Negative with continuation
            uint64_t val = decode_varint(decoder->buffer, decoder->size, &pos);
            value->int_val = -(val + 7);
            decoder->current_pos = pos;
            return TINY_BITS_INT;
        }
}

static inline enum tiny_bits_type _unpack_arr(tiny_bits_unpacker *decoder, uint8_t tag, tiny_bits_value *value){
        size_t pos = decoder->current_pos;
        if (tag < 0b00001111) { // Small array (0-30)
            value->length = tag & 0b00000111;
        } else { // Large array
            value->length = decode_varint(decoder->buffer, decoder->size, &pos) + 7;
            decoder->current_pos = pos;
        }
        return TINY_BITS_ARRAY;
}

static inline enum tiny_bits_type _unpack_map(tiny_bits_unpacker *decoder, uint8_t tag, tiny_bits_value *value){
        size_t pos = decoder->current_pos;
        if (tag < 0x1F) { // Small map (0-14)
            value->length = tag & 0x0F;
        } else { // Large map
            value->length = decode_varint(decoder->buffer, decoder->size, &pos) + 15;
            decoder->current_pos = pos;
        }
        return TINY_BITS_MAP;
}

static inline enum tiny_bits_type _unpack_double(tiny_bits_unpacker *decoder, uint8_t tag, tiny_bits_value *value){
        size_t pos = decoder->current_pos;
        if (tag == TB_F64_TAG) { // Raw double
            uint64_t number = decode_uint64(decoder->buffer + pos);
            value->double_val = itod_bits(number);
            decoder->current_pos += 8;
        } else { // Compressed double
            uint64_t number = decode_varint(decoder->buffer, decoder->size, &pos);
            int order = (tag & 0x0F); 
            double fractional = (double)number / powers[order];
            //fractional /= powers[order];
            if(tag & 0x10) fractional = -fractional;        
            value->double_val = fractional;
            decoder->current_pos = pos;
        }
        return TINY_BITS_DOUBLE;
}

static inline enum tiny_bits_type _unpack_datetime(tiny_bits_unpacker *decoder, uint8_t tag, tiny_bits_value *value){
    size_t pos = decoder->current_pos;
    value->datetime_val.offset = decoder->buffer[pos] * (60*15); // convert offset back to seconds (from multiples of 15 minutes)
    //uint8_t dbl_tag = decoder->buffer[decoder->current_pos++];
    //tiny_bits_value dbl_val;
    //_unpack_double(decoder, dbl_tag, &dbl_val);
    //value->datetime_val.unixtime = dbl_val.double_val;
    uint64_t unixtime = decode_uint64(decoder->buffer + pos + 1);
    value->datetime_val.unixtime = itod_bits(unixtime);
    decoder->current_pos += 9;
    return TINY_BITS_DATETIME;
}

static inline enum tiny_bits_type _unpack_blob(tiny_bits_unpacker *decoder, uint8_t tag, tiny_bits_value *value){
        size_t pos = decoder->current_pos;
        size_t len = decode_varint(decoder->buffer, decoder->size, &pos);
        value->str_blob_val.data =  (const char *)decoder->buffer + pos;
        value->str_blob_val.length = len; 
        decoder->current_pos = pos + len;
        return TINY_BITS_BLOB;
}

static inline enum tiny_bits_type _unpack_str(tiny_bits_unpacker *decoder, uint8_t tag, tiny_bits_value *value){
        size_t pos = decoder->current_pos;
        size_t len;
        if (tag < 0x5F) { // Small string (0-30)
            len = tag & 0x1F;
            value->str_blob_val.data =  (const char *)decoder->buffer + pos;
            value->str_blob_val.length = len; 
            decoder->current_pos = pos + len;
        } else if (tag == 0x5F) { // Large string
            len = decode_varint(decoder->buffer, decoder->size, &pos) + 31;
            value->str_blob_val.data =  (const char *)decoder->buffer + pos;
            value->str_blob_val.length = len; 
            decoder->current_pos = pos + len;
        } else { // Deduplicated (small: < 0x7F, large: 0x7F)
            uint32_t id = (tag < 0x7F) ? (tag & 0x1F) : decode_varint(decoder->buffer, decoder->size, &pos) + 31;
            if (id >= decoder->strings_count) return TINY_BITS_ERROR;
            len = decoder->strings[id].length;
            value->str_blob_val.data = decoder->strings[id].str;
            value->str_blob_val.length = len;
            value->str_blob_val.id = id+1;
            decoder->current_pos = pos; // Update pos after varint
            return TINY_BITS_STR;
        }
        value->str_blob_val.id = 0;
        // Handle new string (not deduplicated)
        if(decoder->strings_count < TB_HASH_CACHE_SIZE && len >= 2 && len <= 128){
            if (decoder->strings_count >= decoder->strings_size) {
                size_t new_size = decoder->strings_size * 2;
                void *new_strings = realloc(decoder->strings, new_size * sizeof(*decoder->strings));
                if (!new_strings) return TINY_BITS_ERROR;
                decoder->strings = new_strings;
                decoder->strings_size = new_size;
            }
            
            decoder->strings[decoder->strings_count].str =  (char *)decoder->buffer + pos;
            decoder->strings[decoder->strings_count].length = len;
            decoder->strings_count++;
            value->str_blob_val.id = -1 * decoder->strings_count;
        }
        return TINY_BITS_STR;
}

/**
 * @brief Unpacks a value and returns its type while setting its value
 *
 * @param decoder The unpacker instance
 * @param[out] value A supplied tiny_bits_value instance
 * 
 * @return enum tiny_bits_type
 *
 * This is the entry point to unpacking tinybits structures. You keep calling
 * this method repeatedly until it returns TINY_BITS_FINISHED when it reaches end of buffer
 * or if it returns TINY_BITS_ERROR if it stumbles on a malformed or unknown structure.
 *
 * TINY_BITS_SEP means the current object was fully unpacked, and that there is potentially another one
 * this is specifically for stream unpacking multiple objects one after the other as they are being recieved 
 *
 * The location of the value you need in the value union will depend on the returned type as follows
 * 
 * TINY_BITS_TRUE, TINY_BITS_FALSE, TINY_BITS_NULL, TINY_BITS_NAN, TINY_BITS_INF & TINY_BITS_N_INF all
 * don't set the value, the type itself is sufficient information for client code to reconstruct the value.
 *
 * TINY_BITS_INT sets value.int_val
 *
 * TINY_BITS_DOUBLE sets value.double_val
 *
 * TINY_BITS_ARRAY and TINY_BITS_MAP both set value.length, for TINY_BITS_ARRAY it means the number of entries,
 * for TINY_BITS_MAP it means the number of key/value pairs. You have to keep calling unpac_value() afterwards to 
 * get all the members of the stored array/map. Please note that tinybits doesn't do size checks on the elements supplied
 * during packing of arrays/maps. It is the responsibility of client code to ensure a 3 element array actually packs 3 elements.
 * 
 * TINY_BITS_STR & TINY_BITS_BLOB both set the value.str_blob_val struct, which has two members, data, a pointer to the string/blob in the buffer and
 * length. Since some returned strings might be deduplicated, they will return the same data pointer and length value for their other instances, there is also an id
 * value in the struct, which will be only set for strings. You can use to quickly determine the state of the strings as follows
 * 
 * A positive value means the string is a duplicate of a previous string, speficially a duplicate of the (id-1)th unpacked, deduplicatable string
 * 
 * A negative value means the sting is not a duplicate but is deduplicatable
 *
 * A zero value means the string is not deduplicatable and no duplicates should be expected (this is a heuristic, as duplicates may still exist)
 */
static inline enum tiny_bits_type unpack_value(tiny_bits_unpacker *decoder, tiny_bits_value *value) {
    if (!decoder || !value || decoder->current_pos >= decoder->size) {
        return (decoder && decoder->current_pos >= decoder->size) ? TINY_BITS_FINISHED : TINY_BITS_ERROR;
    }

    uint8_t tag = decoder->buffer[decoder->current_pos++];
    // Dispatch based on tag
    if ((tag & TB_INT_TAG) == TB_INT_TAG) { // Integers
        return _unpack_int(decoder, tag, value);
    } else if ((tag & TB_STR_TAG) == TB_STR_TAG) { // Strings
        return _unpack_str(decoder, tag, value);
    } else if (tag == TB_NIL_TAG) {
        return TINY_BITS_NULL;
    } else if (tag == TB_NAN_TAG) {
        return TINY_BITS_NAN;
    } else if (tag == TB_INF_TAG) {
        return TINY_BITS_INF;
    } else if (tag == TB_NNF_TAG) {
        return TINY_BITS_N_INF;
    } else if ((tag & TB_DBL_TAG) == TB_DBL_TAG) { // Doubles
        return _unpack_double(decoder, tag, value);
    } else if ((tag & TB_MAP_TAG) == TB_MAP_TAG) { // Maps
        return _unpack_map(decoder, tag, value);
    } else if ((tag & TB_ARR_TAG) == TB_ARR_TAG) { // Arrays
        return _unpack_arr(decoder, tag, value);
    } else if (tag == TB_BLB_TAG) { // Blob
        return _unpack_blob(decoder, tag, value);
    } else if (tag == TB_DTM_TAG) {
        return _unpack_datetime(decoder, tag, value);
    } else if (tag == TB_SEP_TAG) {
        return TINY_BITS_SEP;
    } else if (tag == TB_EXT_TAG) {
        return TINY_BITS_EXT;
    } else if (tag == TB_TRU_TAG) {
        return TINY_BITS_TRUE;
    } else if (tag == TB_FLS_TAG) {
        return TINY_BITS_FALSE;
    }
    return TINY_BITS_ERROR; // Unknown tag
}


/* End unpacker.h */

#endif /* TINY_BIS_H */
