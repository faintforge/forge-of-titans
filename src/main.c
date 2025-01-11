#define WADDLE_IMPLEMENTATION
#include "waddle.h"

#include "window.h"
#include "graphics.h"

static void resize_cb(Window* window, u32 width, u32 height) {
    (void) window;
    WDL_INFO("Resize: %ux%u", width, height);
}

static WDL_Str read_file(WDL_Arena* arena, WDL_Str filename) {
    const char* cstr_filename = wdl_str_to_cstr(arena, filename);
    FILE *fp = fopen(cstr_filename, "rb");
    // Pop off the cstr_filename from the arena since it's no longer needed.
    wdl_arena_pop(arena, filename.len + 1);
    if (fp == NULL) {
        WDL_ERROR("Failed to open file '%.*s'.\n", filename.len, filename.data);
        return (WDL_Str) {0};
    }

    fseek(fp, 0, SEEK_END);
    u32 len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    u8* content = wdl_arena_push(arena, len);
    fread(content, sizeof(u8), len, fp);

    return wdl_str(content, len);
}

i32 main(void) {
    wdl_init();
    WDL_Arena* arena = wdl_arena_create();

    Window* window = window_create(arena, (WindowDesc) {
            .title = "Forge of Titans",
            .width = 1280,
            .height = 720,
            .resize_cb = resize_cb,
            .resizable = false,
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

    typedef struct Vertex Vertex;
    struct Vertex {
        f32 pos[2];
        f32 uv[2];
        GfxColor color;
    };
    Vertex vertices[] = {
        { {-0.5f, -0.5f}, {0.0f, 0.0f}, GFX_COLOR_WHITE },
        { { 0.5f, -0.5f}, {1.0f, 0.0f}, GFX_COLOR_WHITE },
        { {-0.5f,  0.5f}, {0.0f, 1.0f}, GFX_COLOR_WHITE },
        { { 0.5f,  0.5f}, {1.0f, 1.0f}, GFX_COLOR_WHITE },
    };
    GfxBuffer vertex_buffer = gfx_buffer_new((GfxBufferDesc) {
            .size = sizeof(vertices),
            .data = vertices,
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

    GfxVertexArray vertex_array = gfx_vertex_array_new((GfxVertexArrayDesc) {
            .layout = {
                .size = sizeof(Vertex),
                .attribs = {
                    [0] = {
                        .count = 2,
                        .offset = WDL_OFFSET(Vertex, pos),
                    },
                    [1] = {
                        .count = 2,
                        .offset = WDL_OFFSET(Vertex, uv),
                    },
                    [2] = {
                        .count = 4,
                        .offset = WDL_OFFSET(Vertex, color),
                    },
                },
                .attrib_count = 3,
            },
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer
        });

    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
    WDL_Str vertex_source = read_file(scratch.arena, WDL_STR_LIT("assets/shaders/vert.glsl"));
    WDL_Str fragment_source = read_file(scratch.arena, WDL_STR_LIT("assets/shaders/frag.glsl"));
    GfxShader shader = gfx_shader_new(vertex_source, fragment_source);
    wdl_scratch_end(scratch);

    GfxTexture texture = gfx_texture_new((GfxTextureDesc) {
            .data = (u8[]) {
                255, 0, 0,
                0, 255, 0,
                // Padding
                0, 0,
                0, 0, 255,
                255, 0, 255,
                // Padding
                0, 0,
            },
            .width = 2,
            .height = 2,
            .format = GFX_TEXTURE_FORMAT_RGB_U8,
            .sampler = GFX_TEXTURE_SAMPLER_NEAREST,
        });

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

        gfx_clear(gfx_color_rgb_hex(0x6495ed));

        gfx_texture_bind(texture, 0);
        gfx_shader_use(shader);
        gfx_draw_indexed(vertex_array, 6, 0);

        window_swap_buffers(window);
        window_poll_events();
    }

    window_destroy(window);

    wdl_arena_destroy(arena);
    wdl_terminate();
    return 0;
}
