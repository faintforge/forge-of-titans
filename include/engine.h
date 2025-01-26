#ifndef ENGINE_H
#define ENGINE_H

#include "waddle.h"
#include "engine/assman.h"

typedef struct Renderer Renderer;

typedef struct EngineInternal EngineInternal;
typedef struct EngineCtx EngineCtx;
struct EngineCtx {
    WDL_Arena* const frame_arena;
    WDL_Arena* const persistent_arena;
    Renderer* const renderer;
    WDL_Ivec2 window_size;
    void* user_ptr;

    EngineInternal* const _internal;
};

typedef struct ApplicationDesc ApplicationDesc;
struct ApplicationDesc {
    struct {
        WDL_Ivec2 size;
        WDL_Str title;
        b8 resizable;
        b8 vsync;
    } window;

    void (*startup)(EngineCtx* ctx);
    void (*update)(EngineCtx* ctx);
    void (*shutdown)(EngineCtx* ctx);
};

extern i32 engine_run(ApplicationDesc app_desc);

// -- Rendering ---------------------------------------------------------------

typedef struct Texture Texture;
typedef struct Camera Camera;
struct Camera {
    WDL_Ivec2 screen_size;
    WDL_Vec2 pos;
    f32 zoom;
    b8 invert_y;
};

extern WDL_Vec2 world_to_screen_space(WDL_Vec2 world, Camera cam);
extern WDL_Vec2 screen_to_world_space(WDL_Vec2 screen, Camera cam);

//
// Renderer
//

extern void renderer_begin(Renderer* rend, Camera cam);
extern void renderer_end(Renderer* rend);
extern void renderer_draw_quad(Renderer* rend, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color);

#endif // ENGINE_H
