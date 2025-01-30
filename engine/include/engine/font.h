#ifndef FONT_H
#define FONT_H

#include "waddle.h"
#include "graphics.h"

// -- Forward facing API -------------------------------------------------------

typedef struct Font Font;

typedef struct Glyph Glyph;
struct Glyph {
    WDL_Vec2 size;
    WDL_Vec2 offset;
    f32 advance;
    // [0] = Top left
    // [1] = Bottom right
    WDL_Vec2 uv[2];
};

typedef struct FontMetrics FontMetrics;
struct FontMetrics {
    f32 ascent;
    f32 descent;
    f32 linegap;
};

extern Font*       font_create(WDL_Arena* arena, WDL_Str filename);
extern void        font_destroy(Font* font);
extern void        font_set_size(Font* font, u32 size);
extern Glyph       font_get_glyph(Font* font, u32 codepoint);
extern GfxTexture  font_get_atlas(const Font* font);
extern FontMetrics font_get_metrics(const Font* font);
extern f32         font_get_kerning(const Font* font, u32 left_codepoint, u32 right_codepoint);
extern WDL_Vec2    font_measure_string(Font* font, WDL_Str str);

// extern void debug_font_atlas(const Font* font, Quad quad, Camera cam);

#endif // FONT_H
