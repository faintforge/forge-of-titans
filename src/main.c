#define WADDLE_IMPLEMENTATION
#include "waddle.h"

#include "window.h"
#include "graphics.h"
#include "renderer.h"
#include "utils.h"

typedef struct State State;
struct State {
    RenderPipeline pipeline;
};

static Camera camera = {
    .zoom = 15.0f,
};

typedef struct FullscreenQuad FullscreenQuad;
struct FullscreenQuad {
    GfxShader shader;
    GfxVertexArray vertex_array;
};

FullscreenQuad fullscreen_quad_new(void) {
    FullscreenQuad fsq = {0};

    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
    WDL_Str vertex_source = read_file(scratch.arena, WDL_STR_LIT("assets/shaders/blit.vert.glsl"));
    WDL_Str fragment_source = read_file(scratch.arena, WDL_STR_LIT("assets/shaders/blit.frag.glsl"));
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
    WDL_INFO("Resize: %ux%u", size.x, size.y);
}

static void resize_viewport(RenderPassDesc* desc, WDL_Ivec2 size) {
    desc->viewport = size;
}

static void resize_quarter_screen_texture(RenderPassDesc* desc, WDL_Ivec2 size) {
    WDL_Ivec2 small = wdl_iv2_divs(size, 4);
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

static void geometry_pass_execute(const GfxTexture* inputs, u8 input_count, void* user_data) {
    (void) inputs;
    (void) input_count;

    BatchRenderer* br = user_data;

    gfx_clear(GFX_COLOR_BLACK);
    batch_begin(br);

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
                }, camera);
        }
    }

    batch_end(br);
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
    WDL_Arena* arena = wdl_arena_create();

    State state = {0};

    Window* window = window_create(arena, (WindowDesc) {
            .title = "Forge of Titans",
            .size = wdl_iv2(800, 600),
            .resize_cb = resize_cb,
            .resizable = true,
            .vsync = false,
            .user_data = &state,
        });
    if (window == NULL) {
        WDL_ERROR("Window creation failed!");
        return 1;
    } else {
        WDL_INFO("Window created successfully!");
    }
    window_make_current(window);

    if (!gfx_init()) {
        WDL_ERROR("Graphics system failed to initialize!");
        return 1;
    } else {
        WDL_INFO("Graphics system initialized successfully!");
    }

    // -------------------------------------------------------------------------

    FullscreenQuad fsq = fullscreen_quad_new();
    BatchRenderer br = batch_renderer_new(arena, 4096);

    // -------------------------------------------------------------------------

    GfxTexture texture = gfx_texture_new((GfxTextureDesc) {
            .data = NULL,
            .size = wdl_iv2_divs(window_get_size(window), 4),
            .format = GFX_TEXTURE_FORMAT_RGB_U8,
            .sampler = GFX_TEXTURE_SAMPLER_NEAREST,
        });

    RenderPass geometry_pass = render_pass_new((RenderPassDesc) {
            .execute = geometry_pass_execute,
            .user_data = &br,

            .resize = resize_quarter_screen_texture,
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

    // -------------------------------------------------------------------------

    u32 fps = 0;
    f32 fps_timer = 0.0f;

    f32 last = 0.0f;
    while (window_is_open(window)) {
        f32 curr = wdl_os_get_time();
        f32 dt = curr - last;
        last = curr;

        fps++;
        fps_timer += dt;
        if (fps_timer >= 1.0f) {
            WDL_INFO("FPS: %u", fps);
            fps_timer = 0;
            fps = 0;
        }

        camera.screen_size = window_get_size(window);
        render_pipeline_execute(&state.pipeline);

        window_swap_buffers(window);
        window_poll_events();
    }

    window_destroy(window);

    wdl_arena_destroy(arena);
    wdl_terminate();
    return 0;
}
