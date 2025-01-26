#define WADDLE_IMPLEMENTATION
#include "waddle.h"

#include "engine.h"
#include "engine/window.h"
#include "engine/graphics.h"

struct EngineCtx {
    struct {
        WDL_Arena* persistent;
        WDL_Arena* frame[2];
        u8 curr_frame;
    } arenas;
    Window* window;
    void* user_ptr;
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

    // Engine context
    EngineCtx ctx = {
        .arenas = {
            .persistent = persistent,
            .frame = {frame_arenas[0], frame_arenas[1]},
        },
        .window = window,
    };

    app_desc.startup(&ctx);

    while (window_is_open(window)) {
        app_desc.update(&ctx);

        window_swap_buffers(window);
        window_poll_events();
    }

    app_desc.shutdown(&ctx);

    gfx_termiante();
    window_destroy(window);

    wdl_arena_destroy(persistent);
    wdl_arena_destroy(frame_arenas[0]);
    wdl_arena_destroy(frame_arenas[1]);

    wdl_terminate();

    return 0;
}

WDL_Arena* get_persistent_arena(const EngineCtx* ctx) {
    return ctx->arenas.persistent;
}

WDL_Arena* get_frame_arena(const EngineCtx* ctx) {
    return ctx->arenas.frame[ctx->arenas.curr_frame];
}

void set_user_ptr(EngineCtx* ctx, void* user_ptr) {
    ctx->user_ptr = user_ptr;
}

void* get_user_ptr(const EngineCtx* ctx) {
    return ctx->user_ptr;
}
