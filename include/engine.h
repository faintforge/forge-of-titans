#ifndef ENGINE_H
#define ENGINE_H

#include "waddle.h"
#include "engine/window.h"
#include "engine/graphics.h"

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

extern b8 key_down(const EngineCtx* ctx, Key key);
extern b8 key_up(const EngineCtx* ctx, Key key);
extern b8 key_pressed(const EngineCtx* ctx, Key key);
extern b8 key_released(const EngineCtx* ctx, Key key);

extern b8 mouse_button_down(const EngineCtx* ctx, MouseButton button);
extern b8 mouse_button_up(const EngineCtx* ctx, MouseButton button);
extern b8 mouse_button_pressed(const EngineCtx* ctx, MouseButton button);
extern b8 mouse_button_released(const EngineCtx* ctx, MouseButton button);
extern WDL_Vec2 mouse_pos(const EngineCtx* ctx);

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
extern void renderer_draw_quad(Renderer* rend, WDL_Vec2 pivot, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color);

#endif // ENGINE_H
