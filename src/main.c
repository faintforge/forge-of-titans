#define WADDLE_IMPLEMENTATION
#include "waddle.h"

#include "window.h"

static void resize_cb(Window* window, u32 width, u32 height) {
    (void) window;
    WDL_INFO("Resize: %ux%u", width, height);
}

typedef const u8* (*glGetStringFunc)(i32 name);

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

    while (window_is_open(window)) {
        window_swap_buffers(window);
        window_poll_events();
    }

    window_destroy(window);

    wdl_arena_destroy(arena);
    wdl_terminate();
    return 0;
}
