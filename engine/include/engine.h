#ifndef ENGINE_H
#define ENGINE_H

#include "waddle.h"
#include "engine/window.h"
#include "engine/graphics.h"
#include "engine/font.h"

typedef struct Renderer Renderer;

typedef struct ApplicationDesc ApplicationDesc;
struct ApplicationDesc {
    struct {
        WDL_Ivec2 size;
        WDL_Str title;
        b8 resizable;
        b8 vsync;
    } window;

    void (*startup)(void);
    void (*update)(void);
    void (*shutdown)(void);
};

extern i32 engine_run(ApplicationDesc app_desc);

// Getters

extern WDL_Arena* get_presistent_arena(void);
extern WDL_Arena* get_frame_arena(void);
extern Renderer* get_renderer(void);
extern WDL_Ivec2 get_screen_size(void);

// Input

extern b8 key_down(Key key);
extern b8 key_pressed(Key key);
extern b8 key_released(Key key);

extern b8 mouse_button_down(MouseButton button);
extern b8 mouse_button_pressed(MouseButton button);
extern b8 mouse_button_released(MouseButton button);
extern WDL_Vec2 mouse_pos(void);

// -- Rendering ---------------------------------------------------------------

typedef struct Sprite Sprite;
struct Sprite {
    GfxTexture sheet;
    WDL_Ivec2 pos;
    WDL_Ivec2 size;
};

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
extern void renderer_draw_quad_textured(Renderer* rend, WDL_Vec2 pivot, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color, GfxTexture texture);
extern void renderer_draw_quad_textured_uvs(Renderer* rend, WDL_Vec2 pivot, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color, GfxTexture texture, WDL_Vec2 uvs[2]);
extern void renderer_draw_sprite(Renderer* rend, WDL_Vec2 pivot, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color, Sprite sprite);
extern void renderer_draw_text(Renderer* rend, WDL_Str text, Font* font, WDL_Vec2 pivot, WDL_Vec2 pos, Color color);

#endif // ENGINE_H
