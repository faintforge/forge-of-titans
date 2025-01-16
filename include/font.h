#ifndef FONT_H
#define FONT_H

#include "waddle.h"
#include "graphics.h"

typedef struct Font Font;

typedef struct Glyph Glyph;
struct Glyph {
    f32 advance;
    WDL_Vec2 size;
    WDL_Vec2 offset;

    // Normalized UV coordinates.
    // [0] = Top left
    // [1] = Bottom right
    WDL_Vec2 uv[2];
};

typedef struct FontMetrics FontMetrics;
struct FontMetrics {
    i32 ascent;
    i32 descent;
    i32 line_gap;
};

extern Font* font_init(WDL_Arena* arena, WDL_Str ttf_data, b8 sdf);
extern Font* font_init_file(WDL_Arena* arena, WDL_Str filename, b8 sdf);
// Caches all ASCII characters in the range of 32-126 of that size.
extern void font_cache_size(Font* font, u32 size);

extern Glyph font_get_glyph(Font* font, u32 codepoint, u32 size);
extern GfxTexture font_get_atlas(Font* font, u32 size);
extern FontMetrics font_get_metrics(Font* font, u32 size);
extern WDL_Vec2 font_measure_string(Font* font, WDL_Str str, u32 size);

#endif // FONT_H
