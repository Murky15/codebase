#include <stdio.h>

#define ENABLE_ASSERT 1
#define DEBUG 1
#include "base/include.h"
#include "os/include.h"
#include "base/include.c"
#include "os/include.c"

#define COLOR_TYPE_COUNT 6
#define BIT_DEPTH_COUNT 16

#define HUFFMAN_CODE_LENGTH_MAX_CODE_SIZE  7
#define HUFFMAN_MAX_CODE_SIZE 15
#define HUFFMAN_PRIMARY_TABLE_SIZE_THRESHOLD 9
#define HUFFMAN_SECONDARY_TABLE_SIZE_THRESHOLD 6
#define LZ77_NUM_LEN_CODES  29
#define LZ77_NUM_DIST_CODES 30
#define ADLER32_BASE 65521 // Largest prime smaller than 65536

#define VALIDATE_ADLER32 1 

#ifndef VALIDATE_ADLER32
# define VALIDATE_ADLER32 0
#endif

#ifndef HUFFMAN_DEBUG
# define HUFFMAN_DEBUG 0
#endif

typedef struct PNG_Bitmap_RGBA {
    u64 width;
    u64 height;
    u32 *pixels;
} PNG_Bitmap_RGBA;

typedef struct PNG_Chunk {
    u32 length;
    union {
        u32 code;
        u8 c[4];
    } type;
    b32 ancillary;
    u8* data;
} PNG_Chunk;

typedef struct PNG_Chunk_Node {
    struct PNG_Chunk_Node *next;
    PNG_Chunk chunk;
} PNG_Chunk_Node;

typedef struct PNG_Chunk_List {
    PNG_Chunk_Node *first;
    PNG_Chunk_Node *last;
    u64 count;
} PNG_Chunk_List;

typedef union PNG_Palette_Entry {
    struct {
        u8 r,g,b;
    };
    u8 e[3];
} PNG_Palette_Entry;

enum {
    PNG_Grayscale = 0,
    PNG_RGB = 2,
    PNG_Palette = 3,
    PNG_Grayscale_Alpha = 4,
    PNG_RGBA = 6
};

enum {
    PNG_Interlace_Null = 0,
    PNG_Interlace_Adam7 = 1
};

enum {
    Zlib_No_Compression = 0,
    Zlib_Static_Compression = 1,
    Zlib_Dynamic_Compression = 2,
};

typedef struct Zlib_Header {
    u8 cmf;
    u8 flg;
} Zlib_Header;

typedef struct PNG_Critical_Data {
    u32 width;
    u32 height;
    u8 color_type;
    u8 bit_depth;
    u8 compression;
    u8 filter;
    u8 interlace;
    
    b8 palette_present;
    u32 palette_entry_count;
    PNG_Palette_Entry *palette_entries;
    
    u8 *compressed_data;
    u32 compressed_data_size;
    
    b8 end_chunk_processed;
} PNG_Critical_Data;

typedef struct Bit_Stream {
    u64 count;
    u64 buffer;
    u8 *cursor;
} Bit_Stream;

typedef struct Huffman_Dict {
    u32Array sorted_symbols;
    u32Array lengths;
    s32Array table;
    
    u32 max_code_length;
    u32 *codes_per_length;
} Huffman_Dict;

global read_only u8 png_valid_color_config[COLOR_TYPE_COUNT+1][BIT_DEPTH_COUNT+1] = {
    {0,1,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
    {0,1,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1}
};

global read_only u8 png_sample_count[COLOR_TYPE_COUNT+1] = {
    1,0,3,1,2,0,4
};

global read_only u32 lz77_length_map[LZ77_NUM_LEN_CODES] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};

global read_only u8 lz77_extra_length_bits[LZ77_NUM_LEN_CODES] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};

global read_only u32 lz77_dist_map[LZ77_NUM_DIST_CODES] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};

global read_only u8 lz77_extra_distance_bits[LZ77_NUM_DIST_CODES] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

core_function u32
peek_bits (Bit_Stream *stream, u32 count) {
    while (stream->count < count) {
        stream->buffer |= (u32)*stream->cursor++ << stream->count;
        stream->count += 8;
    }
    
    return stream->buffer & ((1u << count)-1);
}

core_function u32
consume_bits (Bit_Stream *stream, u32 count) {
    u32 result = peek_bits(stream, count);
    stream->buffer >>= count;
    stream->count -= count;
    
    return result;
}

core_function u32
reverse_bits (u32 val, u32 count) {
    u32 bit_idx = count;
    u32 flipped = 0;
    while (val > 0) {
        flipped |= (val & 0x1) << --bit_idx;
        val >>= 1;
    }
    
    return flipped;
}

core_function b32
validate_adler32 (u32 adler, String8 inflated) {
    u32 a = 1, b = 0;
    for (u64 i = 0; i < inflated.len; ++i) {
        a = (a + inflated.str[i]) % ADLER32_BASE;
        b = (b + a) % ADLER32_BASE;
    }
    u32 computed_adler = (b << 16) | a;
    
    return computed_adler == adler;
}

// https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art007
core_function Huffman_Dict
huffman_make (Arena *arena, u32Array lengths, u32 max_code_length) {
    assert (max_code_length <= HUFFMAN_PRIMARY_TABLE_SIZE_THRESHOLD + HUFFMAN_SECONDARY_TABLE_SIZE_THRESHOLD);
    
    Temp_Arena scratch = get_scratch(&arena, 1);
    Huffman_Dict result = zero_struct;
    
    u32 *offsets = arena_pushn(scratch.arena, u32, max_code_length+1);
    result.lengths = lengths;
    result.max_code_length = max_code_length;
    result.codes_per_length = arena_pushn(arena, u32, max_code_length+1);
    result.table.count = (1 << max_code_length);
    result.table.array = arena_pushn(arena, s32, result.table.count);
    
    for (u32 sym = 0; sym < lengths.count; ++sym)
        result.codes_per_length[lengths.array[sym]]++;
    assert(result.codes_per_length[0] < lengths.count);
    
    result.sorted_symbols.count = lengths.count - result.codes_per_length[0];
    result.sorted_symbols.array = arena_pushn(arena, u32, result.sorted_symbols.count);
    
    offsets[1] = 0;
    for (u32 len = 1; len < max_code_length; ++len)
        offsets[len+1] = offsets[len] + result.codes_per_length[len];
    
    for (u32 sym = 0; sym < lengths.count; ++sym) {
        if (lengths.array[sym] != 0)
            result.sorted_symbols.array[offsets[lengths.array[sym]]++] = sym;
    }
    
    // @todo: Fine for a first pass, but is an unreal memory hog.
    // I need to switch to multi-level huffman tables, and I would except I have a ton
    // of homework this weekend and I need to get the ball rolling.
    
    // Quick psuedocode:
    // x = Take current bit length - primary threshold
    // create a second-level lookup table with 2^x entries
    // populate it
    // code += x
    // repeat!
    
    u32 code = 0;
    for (u32 i = 0; i < result.sorted_symbols.count; ++i) {
        u32 sym = result.sorted_symbols.array[i];
        u32 next_sym = result.sorted_symbols.array[i+1 < result.sorted_symbols.count ? i+1 : i];
        u32 length = lengths.array[sym];
        u32 next_length = lengths.array[next_sym];
        
        u32 len_diff = max_code_length - length;
        u32 entry_count = (1 << len_diff);
        for (u32 e = 0; e < entry_count; ++e) {
            u32 index = (code << len_diff) | e;
            result.table.array[index] = sym;
        }
        code = (code + 1) << (next_length - length);
    }
    release_scratch(scratch);
    return result;
}

#if HUFFMAN_DEBUG
// @note: puff.c method
core_function s32
huffman_decode_next (Bit_Stream *stream, Huffman_Dict huff) {
    s32 code = 0, first = 0, index = 0;
    for (s32 len = 1; len <= huff.max_code_length; ++len) {
        s32 count = huff.codes_per_length[len];
        code |= consume_bits(stream, 1);
        if (code - count < first)
            return huff.sorted_symbols.array[index + (code - first)];
        
        index += count;
        first += count;
        first <<= 1;
        code  <<= 1;
    }
    
    return -1;
}
#else
core_function s32
huffman_decode_next (Bit_Stream *stream, Huffman_Dict huff) {
    s32 sym = -1;
    u32 index = peek_bits(stream, huff.max_code_length);
    index = reverse_bits(index, huff.max_code_length);
    sym = huff.table.array[index];
    consume_bits(stream, huff.lengths.array[sym]);
    
    return sym;
}
#endif

// https://www.zlib.net/feldspar.html
// https://datatracker.ietf.org/doc/html/rfc1951
// https://datatracker.ietf.org/doc/html/rfc1950
core_function String8
png_zlib_inflate (Arena *arena, String8 deflated) {
    Temp_Arena scratch = get_scratch(&arena, 1);
    
    String8 inflated = zero_struct;
    inflated.str = arena_pushn(arena, u8, 0);
    
    Zlib_Header zlib = *(Zlib_Header*)deflated.str;
    u8 method = (zlib.cmf & 0xF);
    u8 info = ((zlib.cmf & 0xF0) >> 4); // base-2 logarithm of the LZ77 window size, minus eight
    if (method == 8 && info <= 7 && !check_bit(zlib.flg, 5)) {
        Bit_Stream compression_stream = zero_struct;
        compression_stream.cursor = deflated.str + sizeof(Zlib_Header);
        
        u8 final = 0, type = 0;
        while (!final) {
            final = consume_bits(&compression_stream, 1);
            type = consume_bits(&compression_stream, 2);
            
            Huffman_Dict litlen_dict, dist_dict;
            if (type == Zlib_No_Compression) {
                u16 len = consume_bits(&compression_stream, 16);
                u16 nlen = consume_bits(&compression_stream, 16);
                assert(len == ~nlen);
                u8 *dst = arena_pushn(arena, u8, len);
                memory_copy(dst, compression_stream.cursor, len);
                inflated.len += len;
                compression_stream.cursor += len;
                continue;
            } else if (type == Zlib_Dynamic_Compression) {
                u32 hlit  = consume_bits(&compression_stream, 5) + 257;
                u32 hdist = consume_bits(&compression_stream, 5) + 1;
                u32 hclen = consume_bits(&compression_stream, 4) + 4;
                
                local_persist read_only u32 huffman_code_length_sequences[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
                u32Array code_lengths_swizzled;
                code_lengths_swizzled.count = array_count(huffman_code_length_sequences);
                code_lengths_swizzled.array = arena_pushn(scratch.arena, u32, code_lengths_swizzled.count);
                for (u32 i = 0; i < hclen; ++i) {
                    code_lengths_swizzled.array[huffman_code_length_sequences[i]] = consume_bits(&compression_stream, 3);
                }
                Huffman_Dict code_length_dict = huffman_make(scratch.arena, code_lengths_swizzled, HUFFMAN_CODE_LENGTH_MAX_CODE_SIZE);
                
                u32 num_code_lengths = hlit + hdist;
                u32 *code_lengths = arena_pushn(scratch.arena, u32, num_code_lengths);
                u32 rep_count;
                for (u32 i = 0; i < num_code_lengths;) {
                    s32 sym = huffman_decode_next(&compression_stream, code_length_dict);
                    
                    assert(sym != -1);
                    switch (sym) {
                        case 16: {
                            assert(i != 0);
                            rep_count = 3 + consume_bits(&compression_stream, 2);
                            sym = code_lengths[i-1];
                        } break;
                        
                        case 17: {
                            rep_count = 3 + consume_bits(&compression_stream, 3);
                            sym = 0;
                        } break;
                        
                        case 18: {
                            rep_count = 11 + consume_bits(&compression_stream, 7);
                            sym = 0;
                        } break;
                        
                        default: {
                            assert(sym >= 0 && sym <= 15);
                            rep_count = 1;
                        } break;
                    }
                    
                    assert(i + rep_count <= num_code_lengths);
                    for (u32 j = 0; j < rep_count; ++j) {
                        if (i + j < num_code_lengths)
                            code_lengths[i+j] = sym;
                    }
                    i += rep_count;
                }
                assert (code_lengths[256] != 0);
                
                u32Array litlen_data = comp_lit(u32Array, .count = hlit, .array = code_lengths);
                u32Array dist_data = comp_lit(u32Array, .count = hdist, .array = code_lengths + hlit);
                litlen_dict = huffman_make(scratch.arena, litlen_data, HUFFMAN_MAX_CODE_SIZE);
                dist_dict = huffman_make(scratch.arena, dist_data, HUFFMAN_MAX_CODE_SIZE);
            } else {
                fputs("I have yet to come across a png with static huffman codes. If you come across this message, you've got some work to do!\n", stderr);
                goto exit;
            }
            
            while (true) {
                s32 value = huffman_decode_next(&compression_stream, litlen_dict);
                assert(value != -1);
                if (value < 256) {
                    // Copy value (literal byte) to output stream
                    u8 *head = arena_pushn(arena, u8, 1);
                    *head = (u8)value;
                    inflated.len++;
                } else if (value > 256) {
                    u64 length_code = value-257;
                    u64 length = lz77_length_map[length_code];
                    u64 extra_length_bits = lz77_extra_length_bits[length_code];
                    length += consume_bits(&compression_stream, extra_length_bits);
                    
                    s32 dist_code = huffman_decode_next(&compression_stream, dist_dict);
                    assert(dist_code != -1);
                    u64 dist = lz77_dist_map[dist_code];
                    u64 extra_dist_bits = lz77_extra_distance_bits[dist_code];
                    dist += consume_bits(&compression_stream, extra_dist_bits);
                    
                    // Copy length bytes from this position to output stream
                    u64 range_start = inflated.len - dist;
                    String8 data = str8_sub(inflated, range_start, range_start + length);
                    u8 *dest = arena_pushn(arena, u8, length);
                    for (u64 i = 0; i < length; ++i) {
                        u64 wrapped_idx = i % data.len;
                        dest[i] = data.str[wrapped_idx];
                    }
                    inflated.len += length;
                } else {
                    break;
                }
            }
        }
        
#if VALIDATE_ADLER32
        u8 *adler_addr = deflated.str + (deflated.len - 4);
        u32 adler = be_to_le32(adler_addr);
        assert(validate_adler32(adler, inflated));
#endif
    } else {
        fputs("Invalid compression configuration!\n", stderr);
    }
    
    exit:
    release_scratch(scratch);
    return inflated;
}

core_function String8
png_dirty (Arena *arena, PNG_Critical_Data *png, String8 filtered) {
    String8 result = zero_struct;
    assert (png->filter == 0);
    
    u64 bytes_per_sample = (png->bit_depth / 8);
    u64 bytes_per_pixel = bytes_per_sample * png_sample_count[png->color_type];
    u64 scanline_step = png->width * bytes_per_pixel;
    
    u8 *filter_method = filtered.str;
    while (filter_method - filtered.str < filtered.len) {
        printf("Filter byte: %d\n", *filter_method);
        filter_method += scanline_step+1;
    }
    
    return result;
}

core_function void
png_chunk_list_push (Arena *arena, PNG_Chunk_List *list, PNG_Chunk chunk) {
    PNG_Chunk_Node *node = arena_pushn(arena, PNG_Chunk_Node, 1);
    node->chunk = chunk;
    sll_queue_push(list->first, list->last, node);
    list->count++;
}

core_function PNG_Bitmap_RGBA
png_decode (Arena *arena, String8 png_data) {
    PNG_Bitmap_RGBA result = zero_struct;
    Temp_Arena scratch = get_scratch(&arena, 1);
    u8 *c = png_data.str;
    
    // Parse file
    local_persist read_only u8 png_header[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (memory_match(png_header, png_data.str, 8)) {
        PNG_Chunk_List chunks = zero_struct;
        c += array_count(png_header);
        for (;c != png_data.str + png_data.len;) {
            PNG_Chunk chunk = zero_struct;
            chunk.length = be_to_le32(c); c += sizeof(u32);
            chunk.type.code = *(u32*)c; c += sizeof(u32);
            chunk.ancillary = check_bit(chunk.type.c[0], 5);
            chunk.data = c;
            png_chunk_list_push(scratch.arena, &chunks, chunk);
            
            c += chunk.length;
            c += sizeof(u32); // Because this decoder is local, we can skip the CRC
        }
        
        // Process chunks
        PNG_Critical_Data critical_data = zero_struct;
        critical_data.compressed_data = arena_pushn(scratch.arena, u8, 0);
        
        // @todo: Is all of this error checking unnecessary? What are the odds we get fed an invalid PNG file anyway?
        for (PNG_Chunk_Node *n = chunks.first; n; n = n->next) {
            PNG_Chunk chunk = n->chunk;
            if (!chunk.ancillary) {
                if (chunk.type.code == fourcc("IHDR")) {
                    if (n != chunks.first) {
                        fputs("PNG decode error! IHDR is not first!\n", stderr);
                        goto exit;
                    }
                    
                    u8 *c = chunk.data;
                    critical_data.width = be_to_le32(c);  c += sizeof(u32);
                    critical_data.height = be_to_le32(c); c += sizeof(u32);
                    critical_data.bit_depth   = *c++;
                    critical_data.color_type  = *c++;
                    critical_data.compression = *c++;
                    critical_data.filter      = *c++;
                    critical_data.interlace   = *c;
                    
                    if (!png_valid_color_config[critical_data.color_type][critical_data.bit_depth]) {
                        fputs("PNG decode error! Invalid color configuration!\n", stderr);
                        goto exit;
                    }
                } else if (chunk.type.code == fourcc("PLTE")) {
                    critical_data.palette_entry_count = chunk.length / 3;
                    critical_data.palette_entries = (PNG_Palette_Entry*)chunk.data;
                    
                    if (critical_data.bit_depth == 0 || critical_data.compressed_data_size != 0) {
                        fputs("PNG decode error! Incorrect ordering of palette chunk!\n", stderr);
                        goto exit;
                    }
                    
                    if (critical_data.color_type == PNG_Grayscale || critical_data.color_type == PNG_Grayscale_Alpha || critical_data.palette_present) {
                        fputs("PNG decode error! Unnecessary palette chunk!\n", stderr);
                        goto exit;
                    }
                    if (chunk.length % 3 != 0) {
                        fputs("PNG decode error! Invalid palette depth!\n", stderr);
                        goto exit;
                    }
                    if (critical_data.palette_entry_count > (1 << critical_data.bit_depth)) {
                        fputs("PNG decode error! palette entries exceed bit depth!\n", stderr);
                        goto exit;
                    }
                    critical_data.palette_present = true;
                } else if (chunk.type.code == fourcc("IDAT")) {
                    if (critical_data.compression != 0 || critical_data.filter != 0) {
                        fputs("PNG decode error! Unrecognized compression/filter mode\n", stderr);
                        goto exit;
                    }
                    u8 *c = arena_pushn(scratch.arena, u8, chunk.length);
                    memory_copy(c, chunk.data, chunk.length);
                    critical_data.compressed_data_size += chunk.length;
                } else if (chunk.type.code == fourcc("IEND")) {
                    if (n != chunks.last) {
                        fputs("PNG decode error! IEND is not last!\n", stderr);
                        goto exit;
                    }
                    critical_data.end_chunk_processed = true;
                } else {
                    fputs("PNG decode error! Unrecognized critical chunk!\n", stderr);
                    goto exit;
                };
                
            } else {
                printf("PNG: No ancillary chunks are supported yet!\n");
            }
        }
        
        String8 inflated_pixel_data = png_zlib_inflate(scratch.arena, str8(critical_data.compressed_data, critical_data.compressed_data_size));
        String8 unfiltered_pixel_data = png_dirty(scratch.arena, &critical_data, inflated_pixel_data);
        
    } else {
        fprintf(stderr, "Supplied data is not a valid PNG!\n");
    }
    
    exit:
    release_scratch(scratch);
    return result;
}

int
main (void) {
    Temp_Arena scratch = get_scratch(0,0);
    String8 png_data = os_read_file(scratch.arena, str8_lit("W:/assets/dumb/art/tileset/0x72_DungeonTilesetII_v1.7.png"), false);
    //String8 png_data = os_read_file(scratch.arena, str8_lit("W:/code/file/png_tests/guy.png"), false);
    PNG_Bitmap_RGBA parsed_data = png_decode(scratch.arena, png_data);
    
    release_scratch(scratch);
    return 0;
}