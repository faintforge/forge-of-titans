#ifndef FONT_H
#define FONT_H

#include "waddle.h"
#include "renderer.h"

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
    void* (*init)(WDL_Arena* arena, WDL_Str filename);
    void (*terminate)(void* internal);
    FPGlyph (*get_glyph)(void* internal, WDL_Arena* arena, u32 codepoint, u32 size);
};

extern FontProvider font_provider_get_ft2(void);

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

extern QuadtreeAtlas quadtree_atlas_init(WDL_Arena* arena);
extern QuadtreeAtlasNode* quadtree_atlas_insert(QuadtreeAtlas* atlas, WDL_Ivec2 size);
extern void quadtree_atlas_debug_draw(QuadtreeAtlas atlas, Quad quad, Camera cam);

#endif // FONT_H
