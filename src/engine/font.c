#include "engine/font.h"
#include "engine/graphics.h"
#include "engine/utils.h"
#include "engine/renderer.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <stb_truetype.h>

// -- Quadtree packer ----------------------------------------------------------

typedef struct QuadtreeAtlasNode QuadtreeAtlasNode;
struct QuadtreeAtlasNode {
    QuadtreeAtlasNode* children;
    WDL_Ivec2 pos;
    WDL_Ivec2 size;
    b8 occupied;
    b8 split;
};

typedef struct QuadtreeAtlas QuadtreeAtlas;
struct QuadtreeAtlas {
    WDL_Arena* arena;
    QuadtreeAtlasNode root;
    WDL_Ivec2 size;
    u8* bitmap;
};

QuadtreeAtlas quadtree_atlas_init(WDL_Arena* arena, WDL_Ivec2 size) {
    QuadtreeAtlas atlas = {
        .arena = arena,
        .root = {
            .size = size,
        },
        .size = size,
        .bitmap = wdl_arena_push(arena, size.x * size.y),
    };
    return atlas;
}

u32 align_value_up(u32 value, u32 align) {
    u64 aligned = value + align - 1;
    u64 mod = aligned % align;
    aligned = aligned - mod;
    return aligned;
}

QuadtreeAtlasNode* quadtree_atlas_insert_helper(WDL_Arena* arena, QuadtreeAtlasNode* node, WDL_Ivec2 size) {
    if (node == NULL || node->occupied || node->size.x < size.x || node->size.y < size.y) {
        return NULL;
    }

    if (!node->split) {
        if (node->size.x == size.x && node->size.y == size.y) {
            node->occupied = true;
            return node;
        }

        node->children = wdl_arena_push_no_zero(arena, 4 * sizeof(QuadtreeAtlasNode));
        node->split = true;

        // Dynamic split
        if (node->size.x / 2 < size.x || node->size.y / 2 < size.y) {
            node->children[0] = (QuadtreeAtlasNode) {
                .size = size,
                .pos = node->pos,
                .occupied = true,
            };

            {
                WDL_Ivec2 new_size = node->size;
                new_size.x -= size.x;
                new_size.y = size.y;
                WDL_Ivec2 pos = node->pos;
                pos.x += size.x;
                node->children[1] = (QuadtreeAtlasNode) {
                    .size = new_size,
                    .pos = pos,
                };
            }

            {
                WDL_Ivec2 new_size = node->size;
                new_size.x = size.x;
                new_size.y -= size.y;
                WDL_Ivec2 pos = node->pos;
                pos.y += size.y;
                node->children[2] = (QuadtreeAtlasNode) {
                    .size = new_size,
                    .pos = pos,
                };
            }

            return &node->children[0];
        }

        WDL_Ivec2 half_size = wdl_iv2_divs(node->size, 2);
        node->children[0] = (QuadtreeAtlasNode) {
            .size = half_size,
                .pos = node->pos,
        };
        node->children[1] = (QuadtreeAtlasNode) {
            .size = half_size,
                .pos = wdl_iv2(node->pos.x + half_size.x, node->pos.y),
        };
        node->children[2] = (QuadtreeAtlasNode) {
            .size = half_size,
                .pos = wdl_iv2(node->pos.x, node->pos.y + half_size.y),
        };
        node->children[3] = (QuadtreeAtlasNode) {
            .size = half_size,
                .pos = wdl_iv2_add(node->pos, half_size),
        };
    }

    for (u8 i = 0; i < 4; i++) {
        QuadtreeAtlasNode* result = quadtree_atlas_insert_helper(arena, &node->children[i], size);
        if (result != NULL) {
            return result;
        }
    }

    return NULL;
}

QuadtreeAtlasNode* quadtree_atlas_insert(QuadtreeAtlas* atlas, WDL_Ivec2 size) {
    size.x = align_value_up(size.x, 4);
    size.y = align_value_up(size.y, 4);
    return quadtree_atlas_insert_helper(atlas->arena, &atlas->root, size);
}

void quadtree_atlas_debug_draw_helper(WDL_Ivec2 atlas_size, QuadtreeAtlasNode* node, Quad quad, Camera cam) {
    if (node == NULL) {
        return;
    }

    WDL_Vec2 size = wdl_v2(
            (f32) node->size.x / (f32) atlas_size.x,
            (f32) node->size.y / (f32) atlas_size.y
        );
    size = wdl_v2_mul(size, quad.size);

    WDL_Vec2 pos = wdl_v2(
            (f32) node->pos.x / (f32) atlas_size.x,
            -(f32) node->pos.y / (f32) atlas_size.y
        );
    pos = wdl_v2_mul(pos, quad.size);
    pos = wdl_v2_add(pos, quad.pos);

    debug_draw_quad_outline((Quad) {
            .pos = pos,
            .size = size,
            .color = color_rgb_hex(0x808080),
            .pivot = wdl_v2(-0.5f, 0.5f),
        }, cam);

    if (node->split) {
        for (u8 i = 0; i < 4; i++) {
            quadtree_atlas_debug_draw_helper(atlas_size, &node->children[i], quad, cam);
        }
    }
}

void quadtree_atlas_debug_draw(QuadtreeAtlas atlas, Quad quad, Camera cam) {
    debug_draw_quad((Quad) {
            .pos = quad.pos,
            .size = quad.size,
            .pivot = wdl_v2(-0.5f, 0.5f),
            .color = quad.color,
            .texture = quad.texture,
        }, cam);
    quadtree_atlas_debug_draw_helper(atlas.root.size, &atlas.root, quad, cam);
}

// -- Font providers -----------------------------------------------------------

// Font provider glyph
typedef struct FPGlyph FPGlyph;
struct FPGlyph {
    struct {
        u8* buffer;
        WDL_Ivec2 size;
    } bitmap;
    WDL_Vec2 size;
    WDL_Vec2 offset;
    f32 advance;
};

typedef struct FontProvider FontProvider;
struct FontProvider {
    void* (*init)(WDL_Arena* arena, WDL_Str ttf_data);
    void (*terminate)(void* internal);
    u32 (*get_glyph_index)(void* internal, u32 codepoint);
    FPGlyph (*get_glyph)(void* internal, WDL_Arena* arena, u32 glyph_index, u32 size);
    FontMetrics (*get_metrics)(void* internal, u32 size);
    i32 (*get_kerning)(void* internal, u32 left_glyph, u32 right_glyph, u32 size);
};

// -- FreeType2 font provider --------------------------------------------------

typedef struct FT2Internal FT2Internal;
struct FT2Internal {
    FT_Library lib;
    FT_Face face;
};

static void* ft2_init(WDL_Arena*arena, WDL_Str ttf_data) {
    FT2Internal* internal = wdl_arena_push_no_zero(arena, sizeof(FT2Internal));
    FT_Error err = FT_Init_FreeType(&internal->lib);
    if (err != FT_Err_Ok) {
        wdl_error("FreeType2 init error!");
        return NULL;
    }

    FT_New_Memory_Face(internal->lib, ttf_data.data, ttf_data.len, 0, &internal->face);
    if (err != FT_Err_Ok) {
        wdl_error("FreeType2 face init error!");
        return NULL;
    }

    return internal;
}

static void ft2_terminate(void* internal) {
    FT2Internal* ft2 = internal;
    FT_Done_Face(ft2->face);
    FT_Done_FreeType(ft2->lib);
}

static u32 ft2_get_glyph_index(void* internal, u32 codepoint) {
    FT2Internal* ft2 = internal;
    return FT_Get_Char_Index(ft2->face, codepoint);
}

static FPGlyph ft2_get_glyph(void* internal, WDL_Arena* arena, u32 glyph_index, u32 size) {
    (void) arena;

    FT2Internal* ft2 = internal;
    FT_Face face = ft2->face;
    FT_Set_Pixel_Sizes(face, 0, size);

    FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
    if (error != FT_Err_Ok) {
        wdl_error("FreeType2 glyph loading error!");
    }

    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    FT_GlyphSlot slot = face->glyph;
    FT_Bitmap bm = slot->bitmap;
    FT_Glyph_Metrics metrics = slot->metrics;

    FPGlyph glyph = {
        .bitmap = {
            .size = wdl_iv2(bm.width, bm.rows),
            .buffer = bm.buffer,
        },
        .size = wdl_v2(metrics.width >> 6, metrics.height >> 6),
        .offset = wdl_v2(metrics.horiBearingX >> 6, -metrics.horiBearingY >> 6),
        .advance = metrics.horiAdvance >> 6,
    };

    return glyph;
}

static FontMetrics ft2_get_metrics(void* internal, u32 size) {
    FT2Internal* ft2 = internal;
    FT_Face face = ft2->face;
    FT_Set_Pixel_Sizes(face, 0, size);
    FT_Size_Metrics ft_metrics = face->size->metrics;
    FontMetrics metrics = {
        metrics.ascent = ft_metrics.ascender >> 6,
        metrics.descent = ft_metrics.descender >> 6,
        metrics.linegap = ft_metrics.height >> 6,
    };
    return metrics;
}

static i32 ft2_get_kerning(void* internal, u32 left_glyph, u32 right_glyph, u32 size) {
    FT2Internal* ft2 = internal;
    FT_Face face = ft2->face;
    FT_Set_Pixel_Sizes(face, 0, size);
    FT_Vector kern;
    FT_Get_Kerning(face, left_glyph, right_glyph, FT_KERNING_DEFAULT, &kern);
    return kern.x >> 6;
}

static const FontProvider FT2_PROVIDER = {
    .init = ft2_init,
    .terminate = ft2_terminate,
    .get_glyph_index = ft2_get_glyph_index,
    .get_glyph = ft2_get_glyph,
    .get_metrics = ft2_get_metrics,
    .get_kerning = ft2_get_kerning,
};

// -- stb_truetype font provider -----------------------------------------------

typedef struct STBTTInternal STBTTInternal;
struct STBTTInternal {
    stbtt_fontinfo info;
};

static void* fp_stbtt_init(WDL_Arena* arena, WDL_Str ttf_data) {
    STBTTInternal* internal = wdl_arena_push_no_zero(arena, sizeof(STBTTInternal));

    const u8* ttf_cstr = (const u8*) wdl_str_to_cstr(arena, ttf_data);
    if (!stbtt_InitFont(&internal->info, ttf_cstr, stbtt_GetFontOffsetForIndex(ttf_cstr, 0))) {
        wdl_error("STBTT: Init error!");
    }

    return internal;
}

static void fp_stbtt_terminate(void* internal) {
    (void) internal;
}

static u32 fp_stbtt_get_glyph_index(void* internal, u32 codepoint) {
    STBTTInternal* stbtt = internal;
    u32 glyph_index = stbtt_FindGlyphIndex(&stbtt->info, codepoint);
    return glyph_index;
}

static FPGlyph fp_stbtt_get_glyph(void* internal, WDL_Arena* arena, u32 glyph_index, u32 size) {
    STBTTInternal* stbtt = internal;
    i32 advance;
    i32 lsb;
    stbtt_GetGlyphHMetrics(&stbtt->info, glyph_index, &advance, &lsb);

    f32 scale = stbtt_ScaleForPixelHeight(&stbtt->info, size);
    WDL_Ivec2 i0;
    WDL_Ivec2 i1;
    stbtt_GetGlyphBitmapBox(&stbtt->info, glyph_index, scale, scale, &i0.x, &i0.y, &i1.x, &i1.y);
    WDL_Ivec2 bitmap_size = wdl_iv2_sub(i1, i0);
    WDL_Vec2 glyph_size = wdl_v2(bitmap_size.x, bitmap_size.y);
    u32 bitmap_area = bitmap_size.x * bitmap_size.y;
    u8* bitmap = wdl_arena_push_no_zero(arena, bitmap_area);
    stbtt_MakeGlyphBitmap(&stbtt->info, bitmap, bitmap_size.x, bitmap_size.y, bitmap_size.x, scale, scale, glyph_index);

    FPGlyph glyph = {
        .advance = floorf(advance * scale),
        .bitmap = {
            .size = bitmap_size,
            .buffer = bitmap,
        },
        .size = glyph_size,
        // .offset = wdl_v2(i0.x - floorf(lsb * scale), i0.y),
        // .offset = wdl_v2(i0.x, i0.y),
        .offset = wdl_v2(floorf(lsb * scale), i0.y),
    };
    return glyph;
}

static FontMetrics fp_stbtt_get_metrics(void* internal, u32 size) {
    STBTTInternal* stbtt = internal;
    f32 scale = stbtt_ScaleForPixelHeight(&stbtt->info, size);
    i32 ascent;
    i32 descent;
    i32 linegap;
    stbtt_GetFontVMetrics(&stbtt->info, &ascent, &descent, &linegap);
    return (FontMetrics) {
        .ascent = floorf(ascent * scale),
        .descent = floorf(descent * scale),
        .linegap = floorf(linegap * scale),
    };
}

static i32 fp_stbtt_get_kerning(void* internal, u32 left_glyph, u32 right_glyph, u32 size) {
    STBTTInternal* stbtt = internal;
    f32 scale = stbtt_ScaleForPixelHeight(&stbtt->info, size);
    f32 kern = stbtt_GetGlyphKernAdvance(&stbtt->info, left_glyph, right_glyph);
    return floorf(kern * scale);
}

static const FontProvider STBTT_PROVIDER = {
    .init = fp_stbtt_init,
    .terminate = fp_stbtt_terminate,
    .get_glyph_index = fp_stbtt_get_glyph_index,
    .get_glyph = fp_stbtt_get_glyph,
    .get_metrics = fp_stbtt_get_metrics,
    .get_kerning = fp_stbtt_get_kerning,
};

// -- Forward facing API -------------------------------------------------------

typedef struct GlyphInternal GlyphInternal;
struct GlyphInternal {
    Glyph user_glyph;
    u8* bitmap;
    WDL_Ivec2 bitmap_size;
};

typedef struct SizedFont SizedFont;
struct SizedFont {
    u32 size;
    QuadtreeAtlas atlas_packer;
    GfxTexture atlas_texture;
    FontMetrics metrics;
    // Key: u32 (glyph index)
    // Value: GlyphInternal
    WDL_HashMap* glyph_map;
};

struct Font {
    WDL_Arena* arena;
    FontProvider provider;
    WDL_Str ttf_data;

    void* internal;
    u32 curr_size;
    WDL_HashMap* map;
};

SizedFont sized_font_create(Font* font, u32 size) {
    WDL_Ivec2 atlas_size = wdl_iv2(256, 256);
    // Provide a buffer with zeros so the texture is properly cleared.
    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
    u8* zero_buffer = wdl_arena_push(scratch.arena, atlas_size.x * atlas_size.y);
    SizedFont sized = {
        .size = size,
        .atlas_packer = quadtree_atlas_init(font->arena, atlas_size),
        .atlas_texture = gfx_texture_new((GfxTextureDesc) {
                .data = zero_buffer,
                .size = atlas_size,
                .format = GFX_TEXTURE_FORMAT_R_U8,
                .sampler = GFX_TEXTURE_SAMPLER_LINEAR,
            }),
        .metrics = font->provider.get_metrics(font->internal, size),
        .glyph_map = wdl_hm_new(wdl_hm_desc_generic(font->arena, 32, u32, GlyphInternal)),
    };
    wdl_scratch_end(scratch);
    return sized;
}

Font* font_create(WDL_Arena* arena, WDL_Str filename) {
    Font* font = wdl_arena_push_no_zero(arena, sizeof(Font));
    WDL_Str ttf_data = read_file(arena, filename);
    // Push a zero onto the arena so the 'ttf_data' string works as a cstr as
    // well.
    wdl_arena_push(arena, 1);
    FontProvider provider = FT2_PROVIDER;
    // FontProvider provider = STBTT_PROVIDER;
    *font = (Font) {
        .arena = arena,
        .provider = provider,
        .internal = provider.init(arena, ttf_data),
        .ttf_data = ttf_data,
        .map = wdl_hm_new(wdl_hm_desc_generic(arena, 32, u32, SizedFont)),
    };
    return font;
}

void font_destroy(Font* font) {
    font->provider.terminate(font->internal);
}

void font_set_size(Font* font, u32 size) {
    font->curr_size = size;
    if (!wdl_hm_has(font->map, size)) {
        SizedFont sized = sized_font_create(font, size);
        wdl_hm_insert(font->map, size, sized);
    }
}

static void expand_atlas(Font* font, SizedFont* sized) {
    QuadtreeAtlas packer = quadtree_atlas_init(font->arena, wdl_iv2_muls(sized->atlas_packer.size, 2));

    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
    u8* bitmap = wdl_arena_push(scratch.arena, packer.size.x * packer.size.y);

    WDL_HashMapIter iter = wdl_hm_iter_new(sized->glyph_map);
    while (wdl_hm_iter_valid(iter)) {
        GlyphInternal* glyph = wdl_hm_iter_get_valuep(iter);
        QuadtreeAtlasNode* node = quadtree_atlas_insert(&packer, glyph->bitmap_size);
        for (i32 y = 0; y < glyph->bitmap_size.y; y++) {
            u32 atlas_index = node->pos.x + (node->pos.y + y) * packer.size.x;
            u32 glyph_index = y * glyph->bitmap_size.x;
            memcpy(&bitmap[atlas_index], &glyph->bitmap[glyph_index], glyph->bitmap_size.x);
        }

        WDL_Vec2 atlas_size = wdl_v2(packer.size.x, packer.size.y);
        WDL_Vec2 uv_tl = wdl_v2_div(wdl_v2(node->pos.x, node->pos.y), atlas_size);
        WDL_Vec2 uv_br = wdl_v2_div(wdl_v2(node->pos.x + glyph->bitmap_size.x, node->pos.y + glyph->bitmap_size.y), atlas_size);
        glyph->user_glyph.uv[0] = uv_tl;
        glyph->user_glyph.uv[1] = uv_br;

        iter = wdl_hm_iter_next(iter);
    }

    gfx_texture_resize(sized->atlas_texture, (GfxTextureDesc) {
            .data = bitmap,
            .size = packer.size,
            .format = GFX_TEXTURE_FORMAT_R_U8,
            .sampler = GFX_TEXTURE_SAMPLER_LINEAR,
        });
    wdl_scratch_end(scratch);

    sized->atlas_packer = packer;
}

Glyph font_get_glyph(Font* font, u32 codepoint) {
    SizedFont* sized = wdl_hm_getp(font->map, font->curr_size);
    if (sized == NULL) {
        wdl_error("Font of size %u hasn't been created.", font->curr_size);
        return (Glyph) {0};
    }

    u32 glyph_index = font->provider.get_glyph_index(font->internal, codepoint);
    Glyph* _glyph = wdl_hm_getp(sized->glyph_map, glyph_index);
    if (_glyph != NULL) {
        return *_glyph;
    }

    WDL_Scratch scratch = wdl_scratch_begin(&font->arena, 1);
    FPGlyph fp_glyph = font->provider.get_glyph(font->internal, scratch.arena, glyph_index, sized->size);
    u32 bitmap_size = fp_glyph.bitmap.size.x * fp_glyph.bitmap.size.y;
    u8* bitmap = wdl_arena_push(font->arena, bitmap_size);
    memcpy(bitmap, fp_glyph.bitmap.buffer, bitmap_size);
    wdl_scratch_end(scratch);

    QuadtreeAtlasNode* node = quadtree_atlas_insert(&sized->atlas_packer, fp_glyph.bitmap.size);
    // Atlas out of space.
    if (node == NULL) {
        expand_atlas(font, sized);
        return (Glyph) {0};
    }
    gfx_texture_subdata(sized->atlas_texture, (GfxTextureSubDataDesc) {
            .size = fp_glyph.bitmap.size,
            .format = GFX_TEXTURE_FORMAT_R_U8,
            .alignment = 1,
            .pos = node->pos,
            .data = bitmap,
        });

    WDL_Vec2 atlas_size = wdl_v2(sized->atlas_packer.size.x, sized->atlas_packer.size.y);
    WDL_Vec2 uv_tl = wdl_v2_div(wdl_v2(node->pos.x, node->pos.y), atlas_size);
    WDL_Vec2 uv_br = wdl_v2_div(wdl_v2(node->pos.x + fp_glyph.size.x, node->pos.y + fp_glyph.size.y), atlas_size);
    GlyphInternal glyph = {
        .user_glyph = {
            .size = fp_glyph.size,
            .uv = {uv_tl, uv_br},
            .offset = fp_glyph.offset,
            .advance = fp_glyph.advance,
        },
        .bitmap_size = fp_glyph.bitmap.size,
        .bitmap = bitmap,
    };

    wdl_hm_insert(sized->glyph_map, glyph_index, glyph);

    return glyph.user_glyph;
}

GfxTexture font_get_atlas(const Font* font) {
    SizedFont* sized = wdl_hm_getp(font->map, font->curr_size);
    if (sized == NULL) {
        wdl_error("Font of size %u hasn't been created.", font->curr_size);
        return GFX_TEXTURE_NULL;
    }

    return sized->atlas_texture;
}

FontMetrics font_get_metrics(const Font* font) {
    SizedFont* sized = wdl_hm_getp(font->map, font->curr_size);
    if (sized == NULL) {
        wdl_error("Font of size %u hasn't been created.", font->curr_size);
        return (FontMetrics) {0};
    }

    return sized->metrics;
}

f32 font_get_kerning(const Font* font, u32 left_codepoint, u32 right_codepoint) {
    u32 left_glyph = font->provider.get_glyph_index(font->internal, left_codepoint);
    u32 right_glyph = font->provider.get_glyph_index(font->internal, right_codepoint);
    return font->provider.get_kerning(font->internal, left_glyph, right_glyph, font->curr_size);
}

void debug_font_atlas(const Font* font, Quad quad, Camera cam) {
    SizedFont* sized = wdl_hm_getp(font->map, font->curr_size);
    if (sized == NULL) {
        wdl_error("Font of size %u hasn't been created.", font->curr_size);
        return;
    }

    quadtree_atlas_debug_draw(sized->atlas_packer, quad, cam);
}

WDL_Vec2 font_measure_string(Font* font, WDL_Str str) {
    FontMetrics metrics = font_get_metrics(font);
    WDL_Vec2 size = wdl_v2(0.0f, metrics.ascent - metrics.descent);

    for (u32 i = 0; i < str.len; i++) {
        Glyph glyph = font_get_glyph(font, str.data[i]);
        if (i < str.len - 1) {
            size.x += glyph.advance;
        } else {
            size.x += glyph.size.x;
        }
        if (i > 0) {
            size.x += font_get_kerning(font, str.data[i-1], str.data[i]);
        }
    }

    return size;
}
