#ifndef WINDOW_H
#define WINDOW_H

#include "waddle.h"

typedef struct Window Window;

typedef void (*ResizeCallback)(Window* window, WDL_Ivec2 size);

typedef struct WindowDesc WindowDesc;
struct WindowDesc {
    WDL_Ivec2 size;
    const char* title;
    b8 resizable;
    b8 vsync;
    ResizeCallback resize_cb;
    void* user_data;
};

extern Window*   window_create(WDL_Arena* arena, WindowDesc desc);
extern void      window_destroy(Window* window);
extern void      window_poll_events(void);
extern b8        window_is_open(const Window* window);
extern void      window_swap_buffers(Window* window);
extern void      window_make_current(Window* window);
extern WDL_Ivec2 window_get_size(const Window* window);
extern void*     window_get_user_data(const Window* window);

#endif // WINDOW_H
