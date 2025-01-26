#define WADDLE_IMPLEMENTATION
#include "waddle.h"

#include "engine.h"
#include "engine/window.h"
#include "engine/graphics.h"
#include "engine/assman.h"
#include "engine/utils.h"

#define set_const(T, var, value) (*(T*) &(var)) = (value)

static Renderer* renderer_init(WDL_Arena* arena, u32 max_quad_count);

struct EngineInternal {
    WDL_Arena* frame_arenas[2];
    Window* window;
    u8 curr_frame;
};

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
    EngineInternal* internal = wdl_arena_push_no_zero(persistent, sizeof(EngineInternal));
    *internal = (EngineInternal) {
        .frame_arenas = {frame_arenas[0], frame_arenas[1]},
        .window = window,
    };
    EngineCtx ctx = {
        .persistent_arena = persistent,
        .frame_arena = frame_arenas[0],
        .renderer = renderer_init(persistent, 4096),
        .user_ptr = NULL,

        ._internal = internal,
    };

    app_desc.startup(&ctx);

    while (window_is_open(window)) {
        ctx.window_size = window_get_size(window);
        app_desc.update(&ctx);

        window_swap_buffers(window);
        window_poll_events();

        // Update frame arena
        wdl_arena_clear(ctx.frame_arena);
        internal->curr_frame = (internal->curr_frame + 1) % 2;
        set_const(WDL_Arena*, ctx.frame_arena, internal->frame_arenas[internal->curr_frame]);
    }

    app_desc.shutdown(&ctx);
    wdl_debug("Done with app shutdown!");

    assman_terminate();

    gfx_termiante();
    window_destroy(window);

    wdl_arena_destroy(persistent);
    wdl_arena_destroy(frame_arenas[0]);
    wdl_arena_destroy(frame_arenas[1]);

    wdl_terminate();

    return 0;
}

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
    view.b.w = -cam.pos.y;
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
    WDL_Vec4 v4_pos = wdl_v4(screen.x, screen.y, 0.0f, 1.0f);
    WDL_Vec4 world_pos = wdl_m4_mul_vec(proj, v4_pos);
    return wdl_v2(world_pos.x, world_pos.y);
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

void renderer_draw_quad(Renderer* rend, WDL_Vec2 pos, WDL_Vec2 size, f32 rot, Color color) {
    b8 texture_found = false;
    f32 texture_index = 0;
    // if (gfx_texture_is_null(quad.texture)) {
    //     texture_found = true;
    // } else {
    //     for (u8 i = 1; i < br->curr_texture; i++) {
    //         if (br->textures[i].handle == quad.texture.handle) {
    //             texture_index = i;
    //             texture_found = true;
    //             break;
    //         }
    //     }
    // }

    if (rend->curr_quad == rend->max_quad_count ||
            (rend->curr_texture == RENDERER_MAX_TEXTURE_COUNT && !texture_found)) {
        renderer_end(rend);
        renderer_begin(rend, rend->cam);
    }

    // if (!texture_found) {
    //     texture_index = rend->curr_texture;
    //     rend->textures[rend->curr_texture++] = quad.texture;
    // }

    const WDL_Vec2 vert_pos[4] = {
        wdl_v2(-0.5f, -0.5f),
        wdl_v2( 0.5f, -0.5f),
        wdl_v2(-0.5f,  0.5f),
        wdl_v2( 0.5f,  0.5f),
    };

    WDL_Vec2 nw = wdl_v2s(0.0f);
    WDL_Vec2 se = wdl_v2s(1.0f);
    const WDL_Vec2 uv[4] = {
        wdl_v2(nw.x, se.y),
        wdl_v2(se.x, se.y),
        wdl_v2(nw.x, nw.y),
        wdl_v2(se.x, nw.y),
    };

    if (rend->cam.invert_y) {
        pos.y = -pos.y;
    }

    for (u8 i = 0; i < 4; i++) {
        WDL_Vec2 vpos = vert_pos[i];
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
