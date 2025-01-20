#ifndef GAME_H
#define GAME_H

#include "waddle.h"
#include "graphics.h"
#include "renderer.h"

typedef enum EntityFlags {
    ENTITY_FLAG_ALIVE = 1 << 0,
    ENTITY_FLAG_RENDERABLE = 1 << 1,
} EntityFlags;

typedef struct Entity Entity;
struct Entity {
    EntityFlags flags;

    // Transform
    WDL_Vec2 pos;
    WDL_Vec2 size;
    f32 rot;
    WDL_Vec2 pivot;

    // Rendering
    GfxTexture texture;
    GfxColor color;
    b8 use_atlas;
    WDL_Vec2 uvs[2];
};

typedef struct GameState GameState;
struct GameState {
    Camera cam;
    Entity entites[32];
};

#endif // GAME_H
