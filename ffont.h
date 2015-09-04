
#pragma once
#include "fctx.h"

typedef struct __attribute__((__packed__)) FFont {
    uint16_t glyph_count;
    uint16_t unicode_offset;
	fixed16_t units_per_em;
	fixed16_t ascent;
	fixed16_t descent;
} FFont;

typedef struct __attribute__((__packed__)) FGlyph {
    uint16_t path_data_offset;
    uint16_t path_data_length;
    fixed16_t horiz_adv_x;
} FGlyph;

FFont* ffont_create_from_resource(uint32_t resource_id);
void ffont_destroy(FFont* font);
void ffont_debug_log(FFont* font);
FGlyph* ffont_glyph_info(FFont* font, uint16_t unicode);
void* ffont_glyph_outline(FFont* font, FGlyph* glyph);
