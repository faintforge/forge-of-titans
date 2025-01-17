#define WADDLE_IMPLEMENTATION
#include "waddle.h"

#include "window.h"
#include "graphics.h"
#include "renderer.h"
#include "utils.h"
#include "profiler.h"
#include "font.h"

typedef struct State State;
struct State {
    RenderPipeline pipeline;
};

static Camera camera = {
    .zoom = 15.0f,
    .pos = {0.0f, 0.0f},
};

typedef struct FullscreenQuad FullscreenQuad;
struct FullscreenQuad {
    GfxShader shader;
    GfxVertexArray vertex_array;
};

FullscreenQuad fullscreen_quad_new(void) {
    FullscreenQuad fsq = {0};

    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
    WDL_Str vertex_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/blit.vert.glsl"));
    WDL_Str fragment_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/blit.frag.glsl"));
    fsq.shader = gfx_shader_new(vertex_source, fragment_source);
    wdl_scratch_end(scratch);

    f32 verts[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };
    GfxBuffer vertex_buffer = gfx_buffer_new((GfxBufferDesc) {
            .size = sizeof(verts),
            .data = verts,
            .usage = GFX_BUFFER_USAGE_STATIC,
        });

    u32 indices[] = {
        0, 1, 2,
        2, 3, 1,
    };
    GfxBuffer index_buffer = gfx_buffer_new((GfxBufferDesc) {
            .size = sizeof(indices),
            .data = indices,
            .usage = GFX_BUFFER_USAGE_STATIC,
        });

    fsq.vertex_array = gfx_vertex_array_new((GfxVertexArrayDesc) {
            .layout = {
                .size = 4 * sizeof(f32),
                .attribs = {
                    [0] = {
                        .count = 2,
                        .offset = 0,
                    },
                    [1] = {
                        .count = 2,
                        .offset = 2 * sizeof(f32),
                    },
                },
                .attrib_count = 2,
            },
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer,
        });

    return fsq;
}

void fullscreen_quad_draw(FullscreenQuad fsq) {
    gfx_shader_use(fsq.shader);
    gfx_draw_indexed(fsq.vertex_array, 6, 0);
}

static void resize_cb(Window* window, WDL_Ivec2 size) {
    State* state = window_get_user_data(window);
    render_pipeline_resize(&state->pipeline, size);
    wdl_info("Resize: %ux%u", size.x, size.y);
}

static void resize_viewport(RenderPassDesc* desc, WDL_Ivec2 size) {
    desc->viewport = size;
}

static void resize_screen_texture(RenderPassDesc* desc, WDL_Ivec2 size) {
    WDL_Ivec2 small = wdl_iv2_divs(size, 1);
    for (u32 i = 0; i < desc->target_count; i++) {
        gfx_texture_resize(desc->targets[i], (GfxTextureDesc) {
                .data = NULL,
                .size = small,
                .format = GFX_TEXTURE_FORMAT_RGB_U8,
                .sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            });
    }
    resize_viewport(desc, small);
}

typedef struct GeometryPassData GeometryPassData;
struct GeometryPassData {
    BatchRenderer* br;
    GfxShader shader;
    GfxTexture texture;
};

static void geometry_pass_execute(const GfxTexture* inputs, u8 input_count, void* user_data) {
    (void) inputs;
    (void) input_count;

    prof_begin(wdl_str_lit("Geometry pass"));

    GeometryPassData* data = user_data;
    BatchRenderer* br = data->br;

    gfx_clear(GFX_COLOR_BLACK);
    batch_begin(br, data->shader);

    draw_quad(br, (Quad) {
            .size = wdl_v2(5, 5),
            .rotation = 3.14f / 8.0f,
            .color = GFX_COLOR_WHITE,
        }, camera);

    const WDL_Ivec2 grid = wdl_iv2(10, 10);
    for (i32 y = 0; y < grid.y; y++) {
        for (i32 x = 0; x < grid.x; x++) {
            WDL_Vec2 pos = wdl_v2(x, y);
            pos = wdl_v2_sub(pos, wdl_v2_divs(wdl_v2(grid.x - 1, grid.y - 1), 2.0f));

            f32 offset = (x + y) / 5.0f;

            draw_quad(br, (Quad) {
                    .pos = pos,
                    .size = wdl_v2_normalized(wdl_v2s(1.0f)),
                    .rotation = offset + wdl_os_get_time(),
                    .color = gfx_color_hsv((offset + wdl_os_get_time()) * 90.0f, 0.75f, 1.0f),
                    .texture = data->texture,
                }, camera);
        }
    }

    batch_end(br);
    prof_end();
}

static void blit_pass_execute(const GfxTexture* inputs, u8 input_count, void* user_data) {
    for (u8 i = 0; i < input_count; i++) {
        gfx_texture_bind(inputs[i], i);
    }
    FullscreenQuad* fsq = user_data;
    fullscreen_quad_draw(*fsq);
}

i32 main(void) {
    wdl_init();
    profiler_init();

    prof_begin(wdl_str_lit("Startup"));
    WDL_Arena* arena = wdl_arena_create();

    State state = {0};

    prof_begin(wdl_str_lit("Window creation"));
    Window* window = window_create(arena, (WindowDesc) {
            .title = "Forge of Titans",
            .size = wdl_iv2(800, 600),
            .resize_cb = resize_cb,
            .resizable = false,
            .vsync = true,
            .user_data = &state,
        });
    if (window == NULL) {
        wdl_error("Window creation failed!");
        return 1;
    } else {
        wdl_info("Window created successfully!");
    }
    window_make_current(window);
    prof_end();

    prof_begin(wdl_str_lit("Graphics init"));
    if (!gfx_init()) {
        wdl_error("Graphics system failed to initialize!");
        return 1;
    } else {
        wdl_info("Graphics system initialized successfully!");
    }
    prof_end();

    prof_begin(wdl_str_lit("Font create"));
    const u32 font_size = 64;
    Font* roboto = font_init_file(arena, wdl_str_lit("assets/fonts/Roboto/Roboto-Regular.ttf"), false);
    Font* spline_sans = font_init_file(arena, wdl_str_lit("assets/fonts/Spline_Sans/static/SplineSans-Regular.ttf"), false);
    Font* tiny5 = font_init_file(arena, wdl_str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"), false);
    Font* soulside = font_init_file(arena, wdl_str_lit("assets/fonts/soulside/SoulsideBetrayed-3lazX.ttf"), false);
    Font* font = soulside;
    font_cache_size(font, font_size);
    prof_end();

    // -------------------------------------------------------------------------

    FullscreenQuad fsq = fullscreen_quad_new();
    BatchRenderer* br = batch_renderer_new(arena, 8194);

    WDL_Scratch scratch = wdl_scratch_begin(&arena, 1);
    WDL_Str vertex_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/batch.vert.glsl"));
    WDL_Str fragment_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/geometry.frag.glsl"));

    GfxShader geometry_shader = gfx_shader_new(vertex_source, fragment_source);
    gfx_shader_use(geometry_shader);
    i32 samplers[32] = {0};
    for (u32 i = 0; i < wdl_arrlen(samplers); i++) {
        samplers[i] = i;
    }
    gfx_shader_uniform_i32_arr(geometry_shader, wdl_str_lit("textures"), samplers, wdl_arrlen(samplers));

    fragment_source = read_file(scratch.arena, wdl_str_lit("assets/shaders/text.frag.glsl"));
    GfxShader text_shader = gfx_shader_new(vertex_source, fragment_source);
    gfx_shader_use(text_shader);
    gfx_shader_uniform_i32_arr(text_shader, wdl_str_lit("textures"), samplers, wdl_arrlen(samplers));

    wdl_scratch_end(scratch);

    GfxTexture checker_texture;
    {
        const WDL_Ivec2 size = {2, 2};
        u8 pixels[size.x*size.y*4];
        for (i32 y = 0; y < size.y; y++) {
            for (i32 x = 0; x < size.x; x++) {
                u8 color = ((x + y) % 2) * 128 + 127;

                pixels[(x + y * size.x) * 4 + 0] = color;
                pixels[(x + y * size.x) * 4 + 1] = color;
                pixels[(x + y * size.x) * 4 + 2] = color;
                pixels[(x + y * size.x) * 4 + 3] = 255;
            }
        }
        checker_texture = gfx_texture_new((GfxTextureDesc) {
                .data = pixels,
                .size = size,
                .format = GFX_TEXTURE_FORMAT_RGBA_U8,
                .sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            });
    }

    GeometryPassData geometry_data = {
        .br = br,
        .shader = geometry_shader,
        .texture = checker_texture,
    };

    // -------------------------------------------------------------------------

    GfxTexture texture = gfx_texture_new((GfxTextureDesc) {
            .data = NULL,
            .size = window_get_size(window),
            .format = GFX_TEXTURE_FORMAT_RGB_U8,
            .sampler = GFX_TEXTURE_SAMPLER_NEAREST,
        });

    RenderPass geometry_pass = render_pass_new((RenderPassDesc) {
            .execute = geometry_pass_execute,
            .user_data = &geometry_data,

            .resize = resize_screen_texture,
            .screen_size_dependant = true,

            .viewport = gfx_texture_get_size(texture),
            .targets = {texture},
            .target_count = 1,

            .inputs = {},
            .input_count = 0,
        });

    RenderPass blit_pass = render_pass_new((RenderPassDesc) {
            .execute = blit_pass_execute,
            .user_data = &fsq,

            .inputs = {texture},
            .input_count = 1,

            .viewport = window_get_size(window),
            .screen_size_dependant = true,
            .resize = resize_viewport,
        });

    render_pipeline_add_pass(&state.pipeline, geometry_pass);
    render_pipeline_add_pass(&state.pipeline, blit_pass);

    prof_end(); // Startup

    profiler_dump_frame();

    // -------------------------------------------------------------------------

    WDL_Arena* frame_arena = wdl_arena_create();
    DebugCtx dbg = {
        .arena = frame_arena,
    };

    u32 fps = 0;
    f32 fps_timer = 0.0f;

    f32 last = 0.0f;
    u32 frame = 0;
    while (window_is_open(window)) {
        wdl_arena_clear(frame_arena);
        debug_ctx_push(&dbg);

        // debug_draw_quad((Quad) {
        //         .size = wdl_v2s(1.0f),
        //         .color = GFX_COLOR_RED,
        //         .rotation = 3.14 / 4.0f,
        //     }, camera);

        debug_draw_line(wdl_v2s(0.0f), wdl_v2s(1.0f), GFX_COLOR_RED, camera);
        debug_draw_line_angle(wdl_v2s(0.0f), wdl_os_get_time(), 1.0f, GFX_COLOR_RED, camera);

        debug_draw_quad((Quad) {
                .pos = wdl_v2s(0.0f),
                .size = wdl_v2(0.2f, 0.2f),
                .color = GFX_COLOR_GREEN,
            }, camera);

        debug_draw_quad((Quad) {
                .pos = wdl_v2s(1.0f),
                .size = wdl_v2(0.2f, 0.2f),
                .color = GFX_COLOR_GREEN,
            }, camera);

        debug_draw_quad((Quad) {
                .pos = wdl_v2(7.0f, 0.0f),
                .size = wdl_v2(0.1f, 0.1f),
                .color = GFX_COLOR_WHITE,
                .rotation = wdl_os_get_time(),
                .pivot = wdl_v2s(0.25f),
            }, camera);
        debug_draw_quad_outline((Quad) {
                .pos = wdl_v2(7.0f, 0.0f),
                .size = wdl_v2(1.0f, 2.0f),
                .color = GFX_COLOR_GREEN,
                .rotation = wdl_os_get_time(),
                .pivot = wdl_v2s(0.25f),
            }, camera);

        profiler_begin_frame(frame);
        prof_begin(wdl_str_lit("MainLoop"));
        f32 curr = wdl_os_get_time();
        f32 dt = curr - last;
        last = curr;

        fps++;
        fps_timer += dt;
        if (fps_timer >= 1.0f) {
            wdl_info("FPS: %u", fps);
            fps_timer = 0;
            fps = 0;
        }

        prof_begin(wdl_str_lit("Rendering"));
        camera.screen_size = window_get_size(window);
        render_pipeline_execute(&state.pipeline);

        WDL_Ivec2 screen_size = window_get_size(window);
        Camera ui_cam = {
            .screen_size = screen_size,
            .zoom = screen_size.y,
            .pos = wdl_v2(screen_size.x / 2.0f, -screen_size.y / 2.0f),
        };
        batch_begin(br, text_shader);

        FontMetrics metrics = font_get_metrics(font, font_size);
        WDL_Str str = wdl_str_lit("Forge of Titans");
        WDL_Vec2 pos = wdl_v2s(16.0f);
        pos.y += metrics.ascent;
        pos.y = -pos.y;
        for (u32 i = 0; i < str.len; i++) {
            u8 codepoint = str.data[i];
            Glyph glyph = font_get_glyph(font, codepoint, font_size);
            WDL_Vec2 gpos = wdl_v2_add(pos, glyph.offset);
            gpos.y += sinf(-wdl_os_get_time() * 2.0f + i / 5.0f) * metrics.ascent / 2.0f;
            draw_quad_atlas(br, (Quad) {
                    .pos = gpos,
                    .size = glyph.size,
                    .texture = font_get_atlas(font, font_size),
                    .color = gfx_color_hsv(-wdl_os_get_time() * 90.0f + i * 5.0f, 0.75f, 1.0f),
                    .pivot = wdl_v2(-0.5f, 0.5f),
                }, glyph.uv, ui_cam);
            pos.x += glyph.advance;
        }

        debug_ctx_execute(&dbg, br);
        debug_ctx_reset(&dbg);
        batch_end(br);

        prof_end();

        window_swap_buffers(window);
        window_poll_events();

        prof_end();

        profiler_end_frame();
        frame++;
    }

    window_destroy(window);

    wdl_arena_destroy(arena);
    profiler_terminate();
    wdl_terminate();
    return 0;
}
