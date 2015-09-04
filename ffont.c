
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

FGlyph* ffont_glyph_table(FFont* font) {
    return (FGlyph*)(((void*)font) + sizeof(FFont));
}

FGlyph* ffont_glyph_info(FFont* font, uint16_t unicode) {
    if (unicode < font->unicode_offset) {
        return NULL;
    }
    uint16_t glyph_index = unicode - font->unicode_offset;
    if (glyph_index >= font->glyph_count) {
        return NULL;
    }
    FGlyph* glyph_table = ffont_glyph_table(font);
    return glyph_table + glyph_index;
}

void* ffont_glyph_outline(FFont* font, FGlyph* glyph) {
    void* buffer = (void*)font;
    void* path_data = buffer + sizeof(FFont) + font->glyph_count * sizeof(FGlyph) + glyph->path_data_offset;
    return path_data;
}

void ffont_debug_log(FFont* font) {
#if 0
    APP_LOG(APP_LOG_LEVEL_DEBUG, "font gc:%d uo:%d", font->glyph_count, font->unicode_offset);
    FGlyph* glyphs = ffont_glyph_table(font);
    for (uint16_t k = 0; k < font->glyph_count; ++k) {
        FGlyph* g = glyphs + k;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "glyph[%d] \"%c\" |%d| : %d + %d",
            k, (char)(k + font->unicode_offset), g->horiz_adv_x, g->path_data_offset, g->path_data_length);
    }
#endif
}

void ffont_destroy(FFont* font) {
    free(font);
}

