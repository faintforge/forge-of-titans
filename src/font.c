#include "font.h"
#include "graphics.h"
#include "utils.h"
#include "waddle.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define ASCII_FIRST_CHAR 32
#define ASCII_LAST_CHAR 126

// -- Font with fixed size -----------------------------------------------------

#define INITIAL_ATLAS_SIZE wdl_iv2(512, 512)

typedef struct Atlas Atlas;
struct Atlas {
    u8* data;
    WDL_Ivec2 size;
    GfxTexture texture;

    u32 row_height;
    WDL_Ivec2 curr_pos;
};

typedef struct SizedFont SizedFont;
struct SizedFont {
    FontMetrics metrics;
    u32 size;
    Atlas atlas;
    // Key: u32 (codepoint)
    // Value: Glyph
    WDL_HashMap* codepoint_glyph_map;
};

typedef struct SizedFontMap SizedFontMap;
struct SizedFontMap {
    WDL_Arena* arena;
    SizedFont* buckets;
    u32 capacity;
};

struct Font {
    WDL_Arena* arena;
    WDL_Str ttf_data;
    b8 sdf;

    // Key: u32 (size)
    // Value: SizedFont
    WDL_HashMap* map;
};

static SizedFont sized_font_init(Font* font, u32 size) {
    WDL_Str ttf_data = font->ttf_data;

    FT_Library lib;
    FT_Init_FreeType(&lib);

    FT_Face face;
    FT_New_Memory_Face(lib, ttf_data.data, ttf_data.len, 0, &face);

    FT_Set_Pixel_Sizes(face, 0, size);

    FT_Size_Metrics face_metrics = face->size->metrics;
    SizedFont sized = {
        .metrics = {
            .ascent = face_metrics.ascender >> 6,
            .descent = face_metrics.descender >> 6,
            .line_gap = face_metrics.height >> 6,
        },
        .size = size,
        .codepoint_glyph_map = wdl_hm_new(wdl_hm_desc_generic(font->arena, 256, u32, Glyph)),
    };

    Atlas atlas = {
        .data = wdl_arena_push(font->arena, INITIAL_ATLAS_SIZE.x*INITIAL_ATLAS_SIZE.y),
        .size = INITIAL_ATLAS_SIZE,
    };
    for (u32 codepoint = ASCII_FIRST_CHAR; codepoint <= ASCII_LAST_CHAR; codepoint++) {
        u32 glyph_index = FT_Get_Char_Index(face, codepoint);
        FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        FT_Render_Mode mode = !font->sdf ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_SDF;
        FT_Render_Glyph(face->glyph, mode);

        FT_Bitmap bitmap = face->glyph->bitmap;

        if (atlas.curr_pos.x + (i32) bitmap.width > atlas.size.x) {
            atlas.curr_pos.x = 0;
            atlas.curr_pos.y += atlas.row_height;
            atlas.row_height = 0;
        }

        for (u32 y = 0; y < bitmap.rows; y++) {
            for (u32 x = 0; x < bitmap.width; x++) {
                WDL_Ivec2 bitmap_pos = wdl_iv2(x, y);
                WDL_Ivec2 atlas_pos = wdl_iv2_add(atlas.curr_pos, bitmap_pos);

                atlas.data[atlas_pos.x + atlas_pos.y * atlas.size.x] = bitmap.buffer[bitmap_pos.x + bitmap_pos. y * bitmap.width];
            }
        }

        FT_Glyph_Metrics glyph_metrics = face->glyph->metrics;
        WDL_Vec2 nw = wdl_v2(atlas.curr_pos.x, atlas.curr_pos.y);
        WDL_Vec2 se = wdl_v2(atlas.curr_pos.x + bitmap.width, atlas.curr_pos.y + bitmap.rows);
        WDL_Vec2 atlas_size = wdl_v2(atlas.size.x, atlas.size.y);
        nw = wdl_v2_div(nw, atlas_size);
        se = wdl_v2_div(se, atlas_size);
        Glyph glyph = {
            .advance = glyph_metrics.horiAdvance >> 6,
            .size = wdl_v2(glyph_metrics.width >> 6, glyph_metrics.height >> 6),
            .offset = wdl_v2(glyph_metrics.horiBearingX >> 6, glyph_metrics.horiBearingY >> 6),
            .uv = {
                [0] = nw,
                [1] = se,
            },
        };
        wdl_hm_insert(sized.codepoint_glyph_map, codepoint, glyph);

        atlas.curr_pos.x += bitmap.width;
        atlas.row_height = wdl_max(atlas.row_height, bitmap.rows);
    }

    atlas.texture = gfx_texture_new((GfxTextureDesc) {
            .data = atlas.data,
            .size = atlas.size,
            .format = GFX_TEXTURE_FORMAT_R_U8,
            .sampler = GFX_TEXTURE_SAMPLER_LINEAR,
        });

    sized.atlas = atlas;

    FT_Done_Face(face);
    FT_Done_FreeType(lib);

    return sized;
}

Font* font_init(WDL_Arena* arena, WDL_Str ttf_data, b8 sdf) {
    Font* font = wdl_arena_push_no_zero(arena, sizeof(Font));
    *font = (Font) {
        .arena = arena,
        .ttf_data = ttf_data,
        .sdf = sdf,
        .map = wdl_hm_new(wdl_hm_desc_generic(arena, 32, u32, SizedFont)),
    };

    return font;
}

Font* font_init_file(WDL_Arena* arena, WDL_Str filename, b8 sdf) {
    WDL_Str ttf_data = read_file(arena, filename);
    return font_init(arena, ttf_data, sdf);
}

void font_cache_size(Font* font, u32 size) {
    if (!wdl_hm_has(font->map, size)) {
        SizedFont sized = sized_font_init(font, size);
        wdl_hm_insert(font->map, size, sized);
    }
}

Glyph font_get_glyph(Font* font, u32 codepoint, u32 size) {
    SizedFont* sized = wdl_hm_getp(font->map, size);
    Glyph* g = wdl_hm_getp(sized->codepoint_glyph_map, codepoint);
    if (g == NULL) {
        wdl_error("Glyph not found!");
        return (Glyph) {0};
    }
    return *g;
}

GfxTexture font_get_atlas(Font* font, u32 size) {
    SizedFont* sized = wdl_hm_getp(font->map, size);
    return sized->atlas.texture;
}

FontMetrics font_get_metrics(Font* font, u32 size) {
    SizedFont* sized = wdl_hm_getp(font->map, size);
    return sized->metrics;
}

WDL_Vec2 font_measure_string(Font* font, WDL_Str str, u32 size);
