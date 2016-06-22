
#include "ffont.h"

FFont* ffont_create_from_resource(uint32_t resource_id) {
    ResHandle rh = resource_get_handle(resource_id);
    size_t rs = resource_size(rh);
    void* buffer = malloc(rs);
    if (buffer) {
        resource_load(rh, buffer, rs);
        return (FFont*)buffer;
    }
    return NULL;
}

FFont* ffont_load_from_resource_into_buffer(uint32_t resource_id, void* buffer) {
    ResHandle rh = resource_get_handle(resource_id);
    size_t rs = resource_size(rh);
    if (buffer) {
        resource_load(rh, buffer, rs);
        return (FFont*)buffer;
    }
    return NULL;
}

FGlyphRange* ffont_glyph_index(FFont* font) {
    void* buffer = (void*)font;
    void* index = buffer + sizeof(FFont);
    return (FGlyphRange*)index;
}

FGlyph* ffont_glyph_table(FFont* font) {
    void* buffer = (void*)font;
    void* table = buffer + sizeof(FFont)
                + font->glyph_index_length * sizeof(FGlyphRange);
    return (FGlyph*)table;
}

void* ffont_path_data(FFont* font) {
    void* buffer = (void*)font;
    void* path_data = buffer + sizeof(FFont)
                    + font->glyph_index_length * sizeof(FGlyphRange)
                    + font->glyph_table_length * sizeof(FGlyph);
    return path_data;
}

FGlyph* ffont_glyph_info(FFont* font, uint16_t unicode) {
    FGlyphRange* range = ffont_glyph_index(font);
    FGlyphRange* end = range + font->glyph_index_length;
    uint16_t offset = 0;
    while (range < end) {
        if (unicode < range->begin) {
            break;
        }
        if (unicode < range->end) {
            FGlyph* table = ffont_glyph_table(font);
            FGlyph* g = table + offset + (unicode - range->begin);
            /*APP_LOG(APP_LOG_LEVEL_DEBUG, "U+%04X @ U+%04X + %02X",
                unicode, range->begin, unicode - range->begin);*/
            return g;
        }
        offset += (range->end - range->begin);
        ++range;
    }
#if 0
    APP_LOG(APP_LOG_LEVEL_WARNING, "U+%04x no glyph", unicode);
#endif
    return NULL;
}

void* ffont_glyph_outline(FFont* font, FGlyph* glyph) {
    void* path_data = ffont_path_data(font);
    return path_data + glyph->path_data_offset;
}

void ffont_debug_log(FFont* font, uint8_t log_level) {
    if (log_level >= APP_LOG_LEVEL_WARNING && font == NULL) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "font not loaded");
        return;
    }

    if (log_level >= APP_LOG_LEVEL_DEBUG_VERBOSE) {
        APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "so(FF):%d so(FGR):%d so(FG):%d", sizeof(FFont), sizeof(FGlyphRange), sizeof(FGlyph));
        APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "upm:%d a:%d d:%d gil:%d gtl:%d", font->units_per_em, font->ascent, font->descent, font->glyph_index_length, font->glyph_table_length);
    }

    if (log_level >= APP_LOG_LEVEL_DEBUG) {
        uint16_t glyph_count = 0;
        FGlyphRange* index = ffont_glyph_index(font);
        for (uint16_t k = 0; k < font->glyph_index_length; ++k) {
            FGlyphRange* r = index + k;
            glyph_count += (r->end - r->begin);
            APP_LOG(APP_LOG_LEVEL_DEBUG, "U+%04X-%04X", r->begin, r->end - 1);
        }
        APP_LOG(APP_LOG_LEVEL_DEBUG, "gc:%d", glyph_count);
    }

    if (log_level >= APP_LOG_LEVEL_DEBUG_VERBOSE) {
        FGlyph* glyphs = ffont_glyph_table(font);
        for (uint16_t k = 0; k < font->glyph_table_length; ++k) {
            FGlyph* g = glyphs + k;
            APP_LOG(APP_LOG_LEVEL_DEBUG, "glyph[%d] |%d| : %d + %d",
                k, g->horiz_adv_x, g->path_data_offset, g->path_data_length);
        }
    }
}

void ffont_destroy(FFont* font) {
    free(font);
}


 /**
  * Decode the next byte of a UTF-8 byte stream.
  * Initialize state to 0 the before calling this function for the first
  * time for a given stream.  If the returned value is 0, then cp has been
  * set to a valid code point.  Other return values indicate that a multi-
  * byte sequence is in progress, or there was a decoding error.
  *
  * @param byte the byte to decode.
  * @state the current state of the decoder.
  * @cp the decoded unitcode code point.
  * @return the state of the decode process after decoding the byte.
  */
 uint16_t decode_utf8_byte(uint8_t byte, uint16_t* state, uint16_t* cp) {

     /* unicode code points are encoded as follows.
      * U+00000000 – U+0000007F: 0xxxxxxx
      * U+00000080 – U+000007FF: 110xxxxx 10xxxxxx
      * U+00000800 – U+0000FFFF: 1110xxxx 10xxxxxx 10xxxxxx
      * U+00010000 – U+001FFFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      * U+00200000 – U+03FFFFFF: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
      * U+04000000 – U+7FFFFFFF: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
      */

    if (*state == 0 || *state == 6) {
        if (byte < 0b10000000) {        // (<128) ascii character
            *cp = byte;
        } else if (byte < 0b11000000) { // (<192) unexpected continuation byte
            *cp = 0;
            *state = 6;
        } else if (byte < 0b11100000) { // (<224) 2 byte sequence
            *cp = byte & 0b00011111;
            *state = 1;
        } else if (byte < 0b11110000) { // (<240) 3 byte sequence
            *cp = byte & 0b00001111;
            *state = 2;
        } else if (byte < 0b11111000) { // (<248) 4 byte sequence
            *cp = byte & 0b00000111;
            *state = 3;
        } else if (byte < 0b11111100) { // (<252) 5 byte sequence
            *cp = byte & 0b00000011;
            *state = 4;
        } else if (byte < 0b11111110) { // (<254) 6 byte sequence
            *cp = byte & 0b00000001;
            *state = 5;
        }
    } else if (*state < 6) {
        if (byte < 0b11000000) {
            *cp = (*cp << 6) | (byte & 0b00111111);
            *state = *state - 1;
        } else {
            *cp = 0;
            *state = 6;
        }
    }
    return *state;
}
