#ifndef FONT_H
#define FONT_H

#include "waddle.h"
#include "renderer.h"

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
