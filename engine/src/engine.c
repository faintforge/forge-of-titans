#define WADDLE_IMPLEMENTATION
#include "waddle.h"

#include "engine.h"
#include "engine/window.h"
#include "engine/graphics.h"
#include "engine/assman.h"
#include "engine/utils.h"
#include "engine/font.h"

#define set_const(T, var, value) (*(T*) &(var)) = (value)

static Renderer* renderer_init(WDL_Arena* arena, u32 max_quad_count);

typedef struct Engine Engine;
struct Engine {
    struct {
        WDL_Arena* persistent;
        WDL_Arena* frame[2];
        u8 curr_frame;
    } arenas;
    Window* window;
    Renderer* renderer;
};

static Engine engine = {0};

i32 engine_run(ApplicationDesc app_desc) {
    wdl_init(WDL_CONFIG_DEFAULT);

    // Arenas
    WDL_Arena* persistent = wdl_arena_create();
    wdl_arena_tag(persistent, wdl_str_lit("persistent"));

    WDL_Arena* frame_arenas[2] = {wdl_arena_create(), wdl_arena_create()};
    wdl_arena_tag(frame_arenas[0], wdl_str_lit("frame-0"));
    wdl_arena_tag(frame_arenas[1], wdl_str_lit("frame-1"));

    // Windowing
    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
    Window* window = window_create(persistent, (WindowDesc) {
            .title = wdl_str_to_cstr(scratch.arena, app_desc.window.title),
            .size = app_desc.window.size,
            .resizable = app_desc.window.resizable,
            .vsync = app_desc.window.vsync,
        });
    wdl_scratch_end(scratch);
    if (window == NULL) {
        wdl_fatal("Window creation failed!");
        return 1;
    }
    window_make_current(window);

    // Graphics
    b8 success = gfx_init();
    if (!success) {
        wdl_fatal("Graphics system failed to initialize!");
        return 1;
    }

    // Asset manager
    assman_init();

    // Engine context
    engine = (Engine) {
        .arenas = {
            .persistent = persistent,
            .frame = {frame_arenas[0], frame_arenas[1]},
            .curr_frame = 0,
        },
        .window = window,
        .renderer = renderer_init(persistent, 4096),
    };

    app_desc.startup();

    while (window_is_open(window)) {
        gfx_viewport(window_get_size(window));

        app_desc.update();

        window_swap_buffers(window);
        window_poll_events(window);

        // Update frame arena
        engine.arenas.curr_frame = (engine.arenas.curr_frame + 1) % 2;
        wdl_arena_clear(get_frame_arena());
    }

    app_desc.shutdown();

    assman_terminate();

    gfx_termiante();
    window_destroy(window);

    wdl_arena_destroy(persistent);
    wdl_arena_destroy(frame_arenas[0]);
    wdl_arena_destroy(frame_arenas[1]);

    wdl_terminate();

    return 0;
}

// Getters

WDL_Arena* get_presistent_arena(void) { return engine.arenas.persistent; }
WDL_Arena* get_frame_arena(void) { return engine.arenas.frame[engine.arenas.curr_frame]; }
Renderer* get_renderer(void) { return engine.renderer; }
WDL_Ivec2 get_screen_size(void) { return window_get_size(engine.window); }

// Input

b8 key_down(Key key)     { return window_key_down(engine.window, key); }
b8 key_pressed(Key key)  { return window_key_pressed(engine.window, key); }
b8 key_released(Key key) { return window_key_released(engine.window, key); }

b8 mouse_button_down(MouseButton button) { return window_mouse_button_down(engine.window, button); }
b8 mouse_button_pressed(MouseButton button) { return window_mouse_button_pressed(engine.window, button); }
b8 mouse_button_released(MouseButton button) { return window_mouse_button_released(engine.window, button); }
WDL_Vec2 mouse_pos(void) { return window_mouse_pos(engine.window); }

// -- Rendering ---------------------------------------------------------------

static WDL_Mat4 camera_proj(Camera cam) {
    f32 aspect = (f32) cam.screen_size.x / (f32) cam.screen_size.y;
    f32 zoom = cam.zoom * 0.5f;
    WDL_Mat4 proj = wdl_m4_ortho_projection(-aspect * zoom, aspect * zoom, zoom, -zoom, 1.0f, -1.0f);
    return proj;
}

static WDL_Mat4 camera_proj_inv(Camera cam) {
    f32 aspect = (f32) cam.screen_size.x / (f32) cam.screen_size.y;
    f32 zoom = cam.zoom * 0.5f;
    WDL_Mat4 proj = wdl_m4_inv_ortho_projection(-aspect * zoom, aspect * zoom, zoom, -zoom, 1.0f, -1.0f);
    return proj;
}

static WDL_Mat4 camera_view(Camera cam) {
    WDL_Mat4 view = WDL_M4_IDENTITY;
    view.a.w = -cam.pos.x;
    if (cam.invert_y) {
        view.b.w = cam.pos.y;
    } else {
        view.b.w = -cam.pos.y;
    }
    return view;
}

WDL_Vec2 world_to_screen_space(WDL_Vec2 world, Camera cam) {
    WDL_Mat4 proj = camera_proj(cam);
    WDL_Vec4 v4_pos = wdl_v4(world.x, world.y, 0.0f, 1.0f);
    WDL_Vec4 screen_pos = wdl_m4_mul_vec(proj, v4_pos);
    return wdl_v2(screen_pos.x, screen_pos.y);
}

WDL_Vec2 screen_to_world_space(WDL_Vec2 screen, Camera cam) {
    WDL_Mat4 proj = camera_proj_inv(cam);

    WDL_Vec2 sc = wdl_v2(cam.screen_size.x, cam.screen_size.y);
    WDL_Vec2 normalized = screen;
    normalized = wdl_v2_div(normalized, sc);
    normalized = wdl_v2_subs(normalized, 0.5f);
    normalized = wdl_v2_muls(normalized, 2.0f);
    if (!cam.invert_y) {
        normalized.y = -normalized.y;
    }

    WDL_Vec4 v4_pos = wdl_v4(normalized.x, normalized.y, 0.0f, 1.0f);
    WDL_Vec4 world_pos = wdl_m4_mul_vec(proj, v4_pos);
    return wdl_v2_add(wdl_v2(world_pos.x, world_pos.y), cam.pos);
}

//
// Renderer
//

#define RENDERER_MAX_TEXTURE_COUNT 32

typedef struct Vertex Vertex;
struct Vertex {
    WDL_Vec2 pos;
    WDL_Vec2 uv;
    Color color;
    f32 texture_index;
};

struct Renderer {
    u32 max_quad_count;
    u32 curr_quad;

    Vertex* vertices;
    GfxBuffer vertex_buffer;
    GfxVertexArray vertex_array;
    GfxShader shader;
    Camera cam;

    GfxTexture textures[RENDERER_MAX_TEXTURE_COUNT];
    u8 curr_texture;
};

static Renderer* renderer_init(WDL_Arena* arena, u32 max_quad_count) {
    u64 vertices_size = max_quad_count * 4 * sizeof(Vertex);
    GfxBuffer vertex_buffer = gfx_buffer_new((GfxBufferDesc) {
            .size = vertices_size,
            .data = NULL,
            .usage = GFX_BUFFER_USAGE_DYNAMIC,
        });

    // Index buffer
    WDL_Scratch scratch = wdl_scratch_begin(&arena, 1);
    u64 indices_size = max_quad_count * 6 * sizeof(u32);
    u32* indices = wdl_arena_push_no_zero(scratch.arena, indices_size);
    u32 j = 0;
    for (u32 i = 0; i < max_quad_count * 6; i += 6) {
        // First triangle
        indices[i + 0] = j + 0;
        indices[i + 1] = j + 1;
        indices[i + 2] = j + 2;
        // Second triangle
        indices[i + 3] = j + 2;
        indices[i + 4] = j + 3;
        indices[i + 5] = j + 1;
        j += 4;
    }
    GfxBuffer index_buffer = gfx_buffer_new((GfxBufferDesc) {
            .size = indices_size,
            .data = indices,
            .usage = GFX_BUFFER_USAGE_STATIC,
        });
    wdl_scratch_end(scratch);

    // Shaders
    scratch = wdl_scratch_begin(&arena, 1);
    WDL_Str vert_src = read_file(scratch.arena, wdl_str_lit("assets/shaders/batch.vert.glsl"));
    WDL_Str frag_src = read_file(scratch.arena, wdl_str_lit("assets/shaders/batch.frag.glsl"));
    GfxShader shader = gfx_shader_new(vert_src, frag_src);
    i32 samplers[RENDERER_MAX_TEXTURE_COUNT];
    for (u32 i = 0; i < RENDERER_MAX_TEXTURE_COUNT; i++) {
        samplers[i] = i;
    }
    gfx_shader_uniform_i32_arr(shader, wdl_str_lit("textures"), samplers, RENDERER_MAX_TEXTURE_COUNT);
    wdl_scratch_end(scratch);

    // Renderer
    Renderer* rend = wdl_arena_push_no_zero(arena, sizeof(Renderer));
    *rend = (Renderer) {
        .max_quad_count = max_quad_count,

        .vertices = wdl_arena_push_no_zero(arena, vertices_size),
        .vertex_buffer = vertex_buffer,
        .vertex_array = gfx_vertex_array_new((GfxVertexArrayDesc) {
                .layout = {
                    .size = sizeof(Vertex),
                    .attribs = {
                        [0] = {
                            .count = 2,
                            .offset = wdl_offset(Vertex, pos),
                        },
                        [1] = {
                            .count = 2,
                            .offset = wdl_offset(Vertex, uv),
                        },
                        [2] = {
                            .count = 4,
                            .offset = wdl_offset(Vertex, color),
                        },
                        [3] = {
                            .count = 1,
                            .offset = wdl_offset(Vertex, texture_index),
                        },
                    },
                    .attrib_count = 4,
                },
                .vertex_buffer = vertex_buffer,
                .index_buffer = index_buffer,
            }),
        .shader = shader,
        .textures[0] = gfx_texture_new((GfxTextureDesc) {
                .data = (u8[]) { 255, 255, 255, 255 },
                .size = wdl_iv2s(1),
                .format = GFX_TEXTURE_FORMAT_RGBA_U8,
            }),
    };
    return rend;
}

void renderer_begin(Renderer* rend, Camera cam) {
    rend->cam = cam;
    rend->curr_quad = 0;
    rend->curr_texture = 1;
}

void renderer_end(Renderer* rend) {
    gfx_buffer_subdata(rend->vertex_buffer, rend->vertices, rend->curr_quad * 4 * sizeof(Vertex), 0);
    for (u8 i = 0; i < rend->curr_texture; i++) {
        gfx_texture_bind(rend->textures[i], i);
    }
    gfx_shader_use(rend->shader);
    WDL_Mat4 projection = camera_proj(rend->cam);
    WDL_Mat4 view = camera_view(rend->cam);
    gfx_shader_uniform_m4(rend->shader, wdl_str_lit("projection"), projection);
    gfx_shader_uniform_m4(rend->shader, wdl_str_lit("view"), view);
    gfx_draw_indexed(rend->vertex_array, rend->curr_quad * 6, 0);
}

void renderer_draw_quad(Renderer* rend, WDL_Vec2 pivot, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color) {
    renderer_draw_quad_textured(rend, pivot, pos, size, rot, color, GFX_TEXTURE_NULL);
}

void renderer_draw_quad_textured_uvs(Renderer* rend, WDL_Vec2 pivot, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color, GfxTexture texture, WDL_Vec2 uvs[2]) {
    b8 texture_found = false;
    f32 texture_index = 0;
    if (gfx_texture_is_null(texture)) {
        texture_found = true;
    } else {
        for (u8 i = 1; i < rend->curr_texture; i++) {
            if (rend->textures[i].handle == texture.handle) {
                texture_index = i;
                texture_found = true;
                break;
            }
        }
    }

    if (rend->curr_quad == rend->max_quad_count ||
            (rend->curr_texture == RENDERER_MAX_TEXTURE_COUNT && !texture_found)) {
        renderer_end(rend);
        renderer_begin(rend, rend->cam);
    }

    if (!texture_found) {
        texture_index = rend->curr_texture;
        rend->textures[rend->curr_texture++] = texture;
    }

    const WDL_Vec2 vert_pos[4] = {
        wdl_v2(-0.5f, -0.5f),
        wdl_v2( 0.5f, -0.5f),
        wdl_v2(-0.5f,  0.5f),
        wdl_v2( 0.5f,  0.5f),
    };

    WDL_Vec2 nw = uvs[0];
    WDL_Vec2 se = uvs[1];
    const WDL_Vec2 uv[4] = {
        wdl_v2(nw.x, se.y),
        wdl_v2(se.x, se.y),
        wdl_v2(nw.x, nw.y),
        wdl_v2(se.x, nw.y),
    };

    if (rend->cam.invert_y) {
        pos.y = -pos.y;
        pivot.y = -pivot.y;
    }

    pivot = wdl_v2_divs(pivot, 2.0f);
    for (u8 i = 0; i < 4; i++) {
        WDL_Vec2 vpos = vert_pos[i];
        vpos = wdl_v2_sub(vpos, pivot);
        vpos = wdl_v2_mul(vpos, size);
        vpos = wdl_v2(vpos.x * cosf(rot) - vpos.y * sinf(rot),
            vpos.x * sinf(rot) + vpos.y * cosf(rot));
        vpos = wdl_v2_add(vpos, pos);

        rend->vertices[rend->curr_quad * 4 + i] = (Vertex) {
            .pos = vpos,
            .uv = uv[i],
            .color = color,
            .texture_index = texture_index,
        };
    }

    rend->curr_quad++;
}

void renderer_draw_quad_textured(Renderer* rend, WDL_Vec2 pivot, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color, GfxTexture texture) {
    renderer_draw_quad_textured_uvs(rend, pivot, pos, size, rot, color, texture, (WDL_Vec2[2]) {wdl_v2s(0.0f), wdl_v2s(1.0f)});
}

void renderer_draw_sprite(Renderer* rend, WDL_Vec2 pivot, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color, Sprite sprite) {
    WDL_Vec2 sheet_size = wdl_iv2_to_v2(gfx_texture_get_size(sprite.sheet));
    WDL_Vec2 bl = wdl_iv2_to_v2(sprite.pos);
    bl = wdl_v2_div(bl, sheet_size);
    WDL_Vec2 tl = wdl_iv2_to_v2(wdl_iv2_add(sprite.pos, sprite.size));
    tl = wdl_v2_div(tl, sheet_size);
    renderer_draw_quad_textured_uvs(rend, pivot, pos, size, rot, color, sprite.sheet, (WDL_Vec2[2]) {bl, tl});
}

void renderer_draw_text(Renderer* rend, WDL_Str text, Font* font, WDL_Vec2 pivot, WDL_Vec2 pos, Color color) {
    Camera cam = rend->cam;
    f32 aspect = (f32) cam.screen_size.x / (f32) cam.screen_size.y;

    WDL_Vec2 text_size = font_measure_string(font, text);

    FontMetrics metrics = font_get_metrics(font);
    WDL_Vec2 _pos = wdl_v2(0.0f, metrics.ascent);
    pivot = wdl_v2_divs(pivot, 2.0f);
    if (!cam.invert_y) {
        pivot.y = -pivot.y;
    }
    _pos.x -= text_size.x * (pivot.x + 0.5f);
    _pos.y -= text_size.y * (pivot.y + 0.5f);

    GfxTexture atlas = font_get_atlas(font);
    for (u64 i = 0; i < text.len; i++) {
        Glyph glyph = font_get_glyph(font, text.data[i]);
        WDL_Vec2 gpos = _pos;
        gpos = wdl_v2_add(gpos, glyph.offset);
        gpos.x /= cam.screen_size.x;
        gpos.y /= cam.screen_size.y;
        gpos.x *= cam.zoom * aspect;
        gpos.y *= cam.zoom;

        WDL_Vec2 gpivot = wdl_v2s(-1.0f);
        if (!cam.invert_y) {
            gpos.y = -gpos.y;
            gpivot.y = -gpivot.y;
        }

        gpos = wdl_v2_add(gpos, pos);

        WDL_Vec2 size = glyph.size;
        size.x /= cam.screen_size.x;
        size.y /= cam.screen_size.y;
        size.x *= cam.zoom * aspect;
        size.y *= cam.zoom;
        renderer_draw_quad_textured_uvs(rend,
            gpivot,
            gpos,
            size,
            0.0f,
            color,
            atlas,
            glyph.uv);
        _pos.x += glyph.advance;
        if (i > 0) {
            pos.x += font_get_kerning(font, text.data[i - 1], text.data[i]);
        }
    }
}
