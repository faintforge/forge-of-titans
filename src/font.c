#include "font.h"
#include "graphics.h"
#include "renderer.h"
#include "waddle.h"
#include "utils.h"

#include <ft2build.h>
#include FT_FREETYPE_H

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
    u8* bitmap;
};

QuadtreeAtlas quadtree_atlas_init(WDL_Arena* arena) {
    QuadtreeAtlas atlas = {
        .arena = arena,
        .root = {
            .size = wdl_iv2(1024, 1024),
        },
        .bitmap = wdl_arena_push(arena, 1024*1024),
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
            .color = GFX_COLOR_WHITE,
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
    FPGlyph (*get_glyph)(void* internal, WDL_Arena* arena, u32 codepoint, u32 size);
    FontMetrics (*get_metrics)(void* internal, u32 size);
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

static FPGlyph ft2_get_glyph(void* internal, WDL_Arena* arena, u32 codepoint, u32 size) {
    (void) arena;

    FT2Internal* ft2 = internal;
    FT_Face face = ft2->face;
    FT_Set_Pixel_Sizes(face, 0, size);

    FT_Load_Char(face, codepoint, FT_LOAD_RENDER);
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
        .offset = wdl_v2(metrics.horiBearingX >> 6, metrics.horiBearingY >> 6),
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

static const FontProvider FT2_PROVIDER = {
    .init = ft2_init,
    .terminate = ft2_terminate,
    .get_glyph = ft2_get_glyph,
    .get_metrics = ft2_get_metrics,
};

FontProvider font_provider_get_ft2(void) {
    return FT2_PROVIDER;
}

// -- Forward facing API -------------------------------------------------------

typedef struct SizedFont SizedFont;
struct SizedFont {
    void* internal;
    u32 size;
    QuadtreeAtlas atlas_packer;
    GfxTexture atlas_texture;
    WDL_HashMap* glyph_map;
    FontMetrics metrics;
};

struct Font {
    WDL_Arena* arena;
    FontProvider provider;
    WDL_Str ttf_data;

    u32 curr_size;
    WDL_HashMap* map;
};

SizedFont sized_font_create(Font* font, u32 size) {
    void* internal = font->provider.init(font->arena, font->ttf_data);
    SizedFont sized = {
        .internal = internal,
        .size = size,
        .atlas_packer = quadtree_atlas_init(font->arena),
        .atlas_texture = gfx_texture_new((GfxTextureDesc) {
                .size = wdl_iv2(1024, 1024),
                .data = NULL,
                .format = GFX_TEXTURE_FORMAT_R_U8,
                .sampler = GFX_TEXTURE_SAMPLER_LINEAR,
            }),
        .glyph_map = wdl_hm_new(wdl_hm_desc_generic(font->arena, 32, u32, Glyph)),
        .metrics = font->provider.get_metrics(internal, size),
    };
    return sized;
}

Font* font_create(WDL_Arena* arena, WDL_Str filename) {
    Font* font = wdl_arena_push_no_zero(arena, sizeof(Font));
    *font = (Font) {
        .arena = arena,
        .provider = FT2_PROVIDER,
        .ttf_data = read_file(arena, filename),
        .map = wdl_hm_new(wdl_hm_desc_generic(arena, 32, u32, SizedFont)),
    };
    return font;
}

void font_destroy(Font* font) {
    WDL_HashMapIter iter = wdl_hm_iter_new(font->map);
    while (wdl_hm_iter_valid(iter)) {
        SizedFont* sized = wdl_hm_iter_get_valuep(iter);
        font->provider.terminate(sized->internal);
        iter = wdl_hm_iter_next(iter);
    }
}

void font_set_size(Font* font, u32 size) {
    font->curr_size = size;
    SizedFont sized = sized_font_create(font, size);
    wdl_hm_insert(font->map, size, sized);
}

Glyph font_get_glyph(Font* font, u32 codepoint) {
    SizedFont* sized = wdl_hm_getp(font->map, font->curr_size);
    if (sized == NULL) {
        wdl_error("Font of size %u hasn't been created.", font->curr_size);
        return (Glyph) {0};
    }

    Glyph* _glyph = wdl_hm_getp(sized->glyph_map, codepoint);
    if (_glyph != NULL) {
        return *_glyph;
    }

    WDL_Scratch scratch = wdl_scratch_begin(&font->arena, 1);
    FPGlyph fp_glyph = font->provider.get_glyph(sized->internal, scratch.arena, codepoint, sized->size);
    QuadtreeAtlasNode* node = quadtree_atlas_insert(&sized->atlas_packer, fp_glyph.bitmap.size);
    gfx_texture_subdata(sized->atlas_texture, (GfxTextureSubDataDesc) {
            .size = fp_glyph.bitmap.size,
            .format = GFX_TEXTURE_FORMAT_R_U8,
            .alignment = 1,
            .pos = node->pos,
            .data = fp_glyph.bitmap.buffer,
        });
    wdl_scratch_end(scratch);

    WDL_Vec2 uv_tl = wdl_v2_div(wdl_v2(node->pos.x, node->pos.y), wdl_v2(1024, 1024));
    WDL_Vec2 uv_br = wdl_v2_div(wdl_v2(node->pos.x + fp_glyph.size.x, node->pos.y + fp_glyph.size.y), wdl_v2(1024, 1024));
    Glyph glyph = {
        .size = fp_glyph.size,
        .uv = {uv_tl, uv_br},
        .offset = fp_glyph.offset,
        .advance = fp_glyph.advance,
    };

    wdl_hm_insert(sized->glyph_map, codepoint, glyph);

    return glyph;
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
