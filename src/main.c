// #define WADDLE_IMPLEMENTATION
// #include "waddle.h"
//
// #include "engine/window.h"
// #include "engine/graphics.h"
// #include "engine/renderer.h"
// #include "engine/utils.h"
// #include "engine/profiler.h"
// #include "engine/assman.h"
//
// #include "game.h"
//
// typedef struct GeometryPassData GeometryPassData;
// struct GeometryPassData {
//     BatchRenderer* br;
//     GfxShader shader;
//     GameState* game_state;
// };
//
// typedef struct State State;
// struct State {
//     RenderPipeline pipeline;
//     GameState game_state;
// };
//
// typedef struct FullscreenQuad FullscreenQuad;
// struct FullscreenQuad {
//     GfxShader shader;
//     GfxVertexArray vertex_array;
// };
//
// FullscreenQuad fullscreen_quad_new(void) {
//     FullscreenQuad fsq = {0};
//
//     WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
//     WDL_Str vertex_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/blit.vert.glsl"));
//     WDL_Str fragment_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/blit.frag.glsl"));
//     fsq.shader = gfx_shader_new(vertex_source, fragment_source);
//     wdl_scratch_end(scratch);
//
//     f32 verts[] = {
//         -1.0f, -1.0f, 0.0f, 0.0f,
//          1.0f, -1.0f, 1.0f, 0.0f,
//         -1.0f,  1.0f, 0.0f, 1.0f,
//          1.0f,  1.0f, 1.0f, 1.0f,
//     };
//     GfxBuffer vertex_buffer = gfx_buffer_new((GfxBufferDesc) {
//             .size = sizeof(verts),
//             .data = verts,
//             .usage = GFX_BUFFER_USAGE_STATIC,
//         });
//
//     u32 indices[] = {
//         0, 1, 2,
//         2, 3, 1,
//     };
//     GfxBuffer index_buffer = gfx_buffer_new((GfxBufferDesc) {
//             .size = sizeof(indices),
//             .data = indices,
//             .usage = GFX_BUFFER_USAGE_STATIC,
//         });
//
//     fsq.vertex_array = gfx_vertex_array_new((GfxVertexArrayDesc) {
//             .layout = {
//                 .size = 4 * sizeof(f32),
//                 .attribs = {
//                     [0] = {
//                         .count = 2,
//                         .offset = 0,
//                     },
//                     [1] = {
//                         .count = 2,
//                         .offset = 2 * sizeof(f32),
//                     },
//                 },
//                 .attrib_count = 2,
//             },
//             .vertex_buffer = vertex_buffer,
//             .index_buffer = index_buffer,
//         });
//
//     return fsq;
// }
//
// void fullscreen_quad_draw(FullscreenQuad fsq) {
//     gfx_shader_use(fsq.shader);
//     gfx_draw_indexed(fsq.vertex_array, 6, 0);
// }
//
// static void resize_cb(Window* window, WDL_Ivec2 size) {
//     State* state = window_get_user_data(window);
//     render_pipeline_resize(&state->pipeline, size);
//     wdl_info("Resize: %ux%u", size.x, size.y);
// }
//
// static void resize_viewport(RenderPassDesc* desc, WDL_Ivec2 size) {
//     desc->viewport = size;
// }
//
// static void resize_screen_texture(RenderPassDesc* desc, WDL_Ivec2 size) {
//     WDL_Ivec2 small = wdl_iv2_divs(size, 1);
//     for (u32 i = 0; i < desc->target_count; i++) {
//         gfx_texture_resize(desc->targets[i], (GfxTextureDesc) {
//                 .data = NULL,
//                 .size = small,
//                 .format = GFX_TEXTURE_FORMAT_RGB_U8,
//                 .sampler = GFX_TEXTURE_SAMPLER_NEAREST,
//             });
//     }
//     resize_viewport(desc, small);
// }
//
// static void geometry_pass_execute(const GfxTexture* inputs, u8 input_count, void* user_data) {
//     (void) inputs;
//     (void) input_count;
//
//     prof_begin(wdl_str_lit("Geometry pass"));
//
//     GeometryPassData* data = user_data;
//     GameState* gs = data->game_state;
//     BatchRenderer* br = data->br;
//
//     // gfx_clear(GFX_COLOR_BLACK);
//     batch_begin(br, data->shader);
//
//     GfxTexture bg = asset_get(wdl_str_lit("sky_bg"), ASSET_TYPE_TEXTURE, GfxTexture);
//     WDL_Ivec2 size = gfx_texture_get_size(bg);
//     f32 aspect = (f32) size.x / (f32) size.y;
//     draw_quad(br, (Quad) {
//             .size = wdl_v2(gs->cam.zoom * aspect, gs->cam.zoom),
//             .color = GFX_COLOR_WHITE,
//             .texture = bg,
//         }, gs->cam);
//
//     for (u32 i = 0; i < wdl_arrlen(gs->entites); i++) {
//         Entity* ent = &gs->entites[i];
//         if (!(ent->flags & (ENTITY_FLAG_ALIVE | ENTITY_FLAG_RENDERABLE))) {
//             continue;
//         }
//
//         Quad quad = {
//             .pos = ent->pos,
//             .size = ent->size,
//             .pivot = ent->pivot,
//             .rotation = ent->rot,
//             .color = ent->color,
//             .texture = ent->texture,
//         };
//         if (ent->use_atlas) {
//             draw_quad_atlas(br, quad, ent->uvs, gs->cam);
//         } else {
//             draw_quad(br, quad, gs->cam);
//         }
//     }
//
//     batch_end(br);
//     prof_end();
// }
//
// static void blit_pass_execute(const GfxTexture* inputs, u8 input_count, void* user_data) {
//     for (u8 i = 0; i < input_count; i++) {
//         gfx_texture_bind(inputs[i], i);
//     }
//     FullscreenQuad* fsq = user_data;
//     fullscreen_quad_draw(*fsq);
// }
//
// i32 main(void) {
//     wdl_init(WDL_CONFIG_DEFAULT);
//     profiler_init();
//
//     prof_begin(wdl_str_lit("Startup"));
//     WDL_Arena* arena = wdl_arena_create();
//
//     State state = {0};
//
//     prof_begin(wdl_str_lit("Window creation"));
//     Window* window = window_create(arena, (WindowDesc) {
//             .title = "Forge of Titans",
//             .size = wdl_iv2(1280, 720),
//             .resize_cb = resize_cb,
//             .resizable = false,
//             .vsync = true,
//             .user_data = &state,
//         });
//     if (window == NULL) {
//         wdl_error("Window creation failed!");
//         return 1;
//     } else {
//         wdl_info("Window created successfully!");
//     }
//     window_make_current(window);
//     prof_end(); // Window create
//
//     prof_begin(wdl_str_lit("Graphics init"));
//     if (!gfx_init()) {
//         wdl_error("Graphics system failed to initialize!");
//         return 1;
//     } else {
//         wdl_info("Graphics system initialized successfully!");
//     }
//     prof_end(); // Graphics init
//
//     prof_begin(wdl_str_lit("Asset loading"));
//     {
//         assman_init();
//
//         prof_begin(wdl_str_lit("Fonts"));
//         asset_load((AssetDesc) {
//                 .name = wdl_str_lit("roboto"),
//                 .filepath = wdl_str_lit("assets/fonts/Roboto/Roboto-Regular.ttf"),
//                 .type = ASSET_TYPE_FONT,
//             });
//         asset_load((AssetDesc) {
//                 .name = wdl_str_lit("soulside"),
//                 .filepath = wdl_str_lit("assets/fonts/soulside/SoulsideBetrayed-3lazX.ttf"),
//                 .type = ASSET_TYPE_FONT,
//             });
//         asset_load((AssetDesc) {
//                 .name = wdl_str_lit("spline_sans"),
//                 .filepath = wdl_str_lit("assets/fonts/Spline_Sans/static/SplineSans-Regular.ttf"),
//                 .type = ASSET_TYPE_FONT,
//             });
//         asset_load((AssetDesc) {
//                 .name = wdl_str_lit("tiny5"),
//                 .filepath = wdl_str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"),
//                 .type = ASSET_TYPE_FONT,
//             });
//         prof_end(); // Fonts
//
//         prof_begin(wdl_str_lit("Textures"));
//         asset_load((AssetDesc) {
//                 .name = wdl_str_lit("player"),
//                 .filepath = wdl_str_lit("assets/textures/player.png"),
//                 .type = ASSET_TYPE_TEXTURE,
//                 .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
//             });
//         asset_load((AssetDesc) {
//                 .name = wdl_str_lit("sky_bg"),
//                 .filepath = wdl_str_lit("assets/textures/sky_bg.png"),
//                 .type = ASSET_TYPE_TEXTURE,
//                 .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
//             });
//         prof_end(); // Textures
//     }
//     prof_end(); // Asset loading
//
//     // -------------------------------------------------------------------------
//
//     FullscreenQuad fsq = fullscreen_quad_new();
//     BatchRenderer* br = batch_renderer_new(arena, 8194);
//
//     WDL_Scratch scratch = wdl_scratch_begin(&arena, 1);
//     WDL_Str vertex_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/batch.vert.glsl"));
//     WDL_Str fragment_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/geometry.frag.glsl"));
//
//     GfxShader geometry_shader = gfx_shader_new(vertex_source, fragment_source);
//     gfx_shader_use(geometry_shader);
//     i32 samplers[32] = {0};
//     for (u32 i = 0; i < wdl_arrlen(samplers); i++) {
//         samplers[i] = i;
//     }
//     gfx_shader_uniform_i32_arr(geometry_shader, wdl_str_lit("textures"), samplers, wdl_arrlen(samplers));
//
//     fragment_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/text.frag.glsl"));
//     GfxShader text_shader = gfx_shader_new(vertex_source, fragment_source);
//     gfx_shader_use(text_shader);
//     gfx_shader_uniform_i32_arr(text_shader, wdl_str_lit("textures"), samplers, wdl_arrlen(samplers));
//
//     wdl_scratch_end(scratch);
//
//     // -------------------------------------------------------------------------
//
//     GfxTexture geometry_pass_target = gfx_texture_new((GfxTextureDesc) {
//             .data = NULL,
//             .size = window_get_size(window),
//             .format = GFX_TEXTURE_FORMAT_RGB_U8,
//             .sampler = GFX_TEXTURE_SAMPLER_NEAREST,
//         });
//
//     GeometryPassData gp_data = {
//         .br = br,
//         .shader = geometry_shader,
//         .game_state = &state.game_state,
//     };
//     RenderPass geometry_pass = render_pass_new((RenderPassDesc) {
//             .execute = geometry_pass_execute,
//             .user_data = &gp_data,
//
//             .resize = resize_screen_texture,
//             .screen_size_dependant = true,
//
//             .viewport = gfx_texture_get_size(geometry_pass_target),
//             .targets = {geometry_pass_target},
//             .target_count = 1,
//
//             .inputs = {},
//             .input_count = 0,
//         });
//
//     RenderPass blit_pass = render_pass_new((RenderPassDesc) {
//             .execute = blit_pass_execute,
//             .user_data = &fsq,
//
//             .inputs = {geometry_pass_target},
//             .input_count = 1,
//
//             .viewport = window_get_size(window),
//             .screen_size_dependant = true,
//             .resize = resize_viewport,
//         });
//
//     render_pipeline_add_pass(&state.pipeline, geometry_pass);
//     render_pipeline_add_pass(&state.pipeline, blit_pass);
//
//     prof_end(); // Startup
//
//     profiler_dump_frame();
//
//     // Player
//     state.game_state.cam = (Camera) {
//         .screen_size = window_get_size(window),
//         .zoom = 12.0f,
//     };
//
//     GfxTexture player = asset_get(wdl_str_lit("player"), ASSET_TYPE_TEXTURE, GfxTexture);
//     WDL_Ivec2 size = gfx_texture_get_size(player);
//     f32 aspect = (f32) size.x / (f32) size.y;
//     state.game_state.entites[0] = (Entity) {
//         .size = wdl_v2(aspect, 1.0f),
//         .color = GFX_COLOR_WHITE,
//         .flags = ENTITY_FLAG_ALIVE | ENTITY_FLAG_RENDERABLE,
//         .texture = player,
//     };
//
//     // -------------------------------------------------------------------------
//
//     WDL_Arena* frame_arena = wdl_arena_create();
//     DebugCtx dbg = {
//         .arena = frame_arena,
//     };
//
//     u32 fps = 0;
//     f32 fps_timer = 0.0f;
//
//     f32 last = 0.0f;
//     u32 frame = 0;
//     while (window_is_open(window)) {
//         profiler_begin_frame(frame);
//         prof_begin(wdl_str_lit("MainLoop"));
//         f32 curr = wdl_os_get_time();
//         f32 dt = curr - last;
//         last = curr;
//
//         fps++;
//         fps_timer += dt;
//         if (fps_timer >= 1.0f) {
//             wdl_info("FPS: %u", fps);
//             fps_timer = 0;
//             fps = 0;
//         }
//
//         wdl_arena_clear(frame_arena);
//         debug_ctx_push(&dbg);
//
//         state.game_state.cam.screen_size = window_get_size(window);
//
//         prof_begin(wdl_str_lit("Rendering"));
//         render_pipeline_execute(&state.pipeline);
//
//         batch_begin(br, geometry_shader);
//         debug_ctx_execute(&dbg, br);
//         debug_ctx_reset(&dbg);
//         batch_end(br);
//
//         prof_end(); // Rendering
//
//         window_swap_buffers(window);
//         window_poll_events();
//
//         prof_end(); // Main loop
//
//         profiler_end_frame();
//         frame++;
//     }
//
//     assman_terminate();
//
//     window_destroy(window);
//
//     wdl_arena_destroy(arena);
//     profiler_terminate();
//     wdl_terminate();
//     return 0;
// }

#include "engine.h"
#include "engine/assman.h"
#include "waddle.h"

typedef struct Entity Entity;
struct Entity {
    // Transform
    WDL_Vec2 pos;
    WDL_Vec2 size;
    f32 rot;

    // Rendering
    Texture* texture;
    Color color;
};

typedef struct Game Game;
struct Game {
    f32 dt;
    Entity entities[512];
};

void app_startup(EngineCtx* ctx) {
    WDL_Arena* persistent = ctx->persistent_arena;
    Game* game = wdl_arena_push(persistent, sizeof(Game));
    ctx->user_ptr = game;

    // Assets
    asset_load((AssetDesc) {
            .name = wdl_str_lit("player"),
            .filepath = wdl_str_lit("assets/textures/player.png"),
            .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            .type = ASSET_TYPE_TEXTURE,
        });
    asset_load((AssetDesc) {
            .name = wdl_str_lit("tiny5"),
            .filepath = wdl_str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"),
            .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            .type = ASSET_TYPE_FONT,
        });

    // Player
    game->entities[0] = (Entity) {
        .pos = wdl_v2(0.0f, 0.0f),
        .size = wdl_v2(1.0f, 2.0f),
        .rot = 0.0f,

        .texture = asset_get(wdl_str_lit("player"), ASSET_TYPE_TEXTURE, Texture*),
        .color = COLOR_WHITE,
    };
}

void app_update(EngineCtx* ctx) {
    Game* game = ctx->user_ptr;
    Renderer* renderer = ctx->renderer;

    // Delta time
    static f32 last = 0.0f;
    f32 curr = wdl_os_get_time();
    game->dt = curr - last;
    last = curr;

    // FPS
    static f32 fps_timer = 0.0f;
    static u32 fps = 0;
    fps_timer += game->dt;
    fps++;
    if (fps_timer >= 1.0f) {
        wdl_info("FPS: %u", fps);
        fps = 0;
        fps_timer = 0.0f;
    }

    // Rendering
    Camera cam = {
        .zoom = 16.0f,
        .pos = wdl_v2(0.0f, 0.0f),
        .invert_y = false,
        .screen_size = ctx->window_size,
    };

    renderer_begin(renderer, cam);

    f32 aspect = (f32) cam.screen_size.x / (f32) cam.screen_size.y;
    WDL_Vec2 full_screen_quad_size = wdl_v2(aspect * cam.zoom, cam.zoom);
    renderer_draw_quad(renderer, wdl_v2s(0.0f), full_screen_quad_size, 0.0, COLOR_BLACK);

    renderer_draw_quad(renderer, wdl_v2(0.0f, 0.0f), wdl_v2s(1.0f), 0.0f, COLOR_WHITE);

    renderer_end(renderer);
}

void app_shutdown(EngineCtx* ctx) {
    (void) ctx;
    wdl_dump_arena_metrics();
}

// NOTE: GLFW calls any function called shutdown.
// void shutdown(EngineCtx* ctx) {
//     (void) ctx;
//     wdl_debug("Called from GLFW!");
// }

i32 main(void) {
    return engine_run((ApplicationDesc) {
            .window = {
                .size = wdl_iv2(800, 600),
                .title = wdl_str_lit("Forge of Titans"),
                .resizable = false,
                .vsync = true,
            },
            .startup = app_startup,
            .update = app_update,
            .shutdown = app_shutdown,
        });
}
