#include "engine/window.h"
#include "waddle.h"

#include <GLFW/glfw3.h>

typedef struct Button Button;
struct Button {
    b8 down;
    b8 first;
};

struct Window {
    GLFWwindow* handle;
    b8 is_open;
    WDL_Ivec2 size;
    b8 vsync;
    void* user_data;

    ResizeCallback resize_cb;

    Button keyboard[GLFW_KEY_LAST];
    struct {
        WDL_Vec2 pos;
        Button button[MOUSE_BUTTON_COUNT];
    } mouse;
};

static void close_cb(GLFWwindow* handle) {
    Window* window = glfwGetWindowUserPointer(handle);
    window->is_open = false;
}

static void internal_resize_cb(GLFWwindow* handle, i32 width, i32 height) {
    Window* window = glfwGetWindowUserPointer(handle);
    window->size = wdl_iv2(width, height);
    if (window->resize_cb != NULL) {
        window->resize_cb(window, window->size);
    }
}

static void key_cb(GLFWwindow* handle, i32 key, i32 scancode, i32 action, i32 mods) {
    (void) scancode;
    (void) mods;
    Window* window = glfwGetWindowUserPointer(handle);
    switch (action) {
        case GLFW_PRESS:
            window->keyboard[key].down = true;
            window->keyboard[key].first = true;
            break;
        case GLFW_RELEASE:
            window->keyboard[key].down = false;
            window->keyboard[key].first = true;
            break;
        default:
            break;
    }
}

static void mouse_pos_cb(GLFWwindow* handle, f64 xpos, f64 ypos) {
    Window* window = glfwGetWindowUserPointer(handle);
    window->mouse.pos = wdl_v2(xpos, ypos);
}

static void mouse_button_cb(GLFWwindow* handle, i32 button, i32 action, i32 mods) {
    (void) mods;
    Window* window = glfwGetWindowUserPointer(handle);
    switch (action) {
        case GLFW_PRESS:
            window->mouse.button[button].down = true;
            window->mouse.button[button].first = true;
            break;
        case GLFW_RELEASE:
            window->mouse.button[button].down = false;
            window->mouse.button[button].first = true;
            break;
        default:
            break;
    }
}

Window* window_create(WDL_Arena* arena, WindowDesc desc) {
    if (!glfwInit()) {
        return NULL;
    }

    Window* window = wdl_arena_push_no_zero(arena, sizeof(Window));

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable);
    *window = (Window) {
        .handle = glfwCreateWindow(desc.size.x,
                desc.size.y,
                desc.title,
                NULL,
                NULL),
        .is_open = true,
        .size = desc.size,
        .vsync = desc.vsync,
        .resize_cb = desc.resize_cb,
        .user_data = desc.user_data,
    };
    if (window->handle == NULL) {
        return NULL;
    }
    glfwSetWindowUserPointer(window->handle, window);
    glfwSetWindowCloseCallback(window->handle, close_cb);
    glfwSetFramebufferSizeCallback(window->handle, internal_resize_cb);
    glfwSetKeyCallback(window->handle, key_cb);
    glfwSetCursorPosCallback(window->handle, mouse_pos_cb);
    glfwSetMouseButtonCallback(window->handle, mouse_button_cb);

    glfwMakeContextCurrent(window->handle);
    glfwSwapInterval(desc.vsync);
    glfwMakeContextCurrent(NULL);

    return window;
}

void window_destroy(Window* window) {
    glfwDestroyWindow(window->handle);
    glfwTerminate();
}

void window_poll_events(Window* window) {
    for (u32 i = 0; i < wdl_arrlen(window->keyboard); i++) {
        window->keyboard[i].first = false;
    }
    for (u32 i = 0; i < wdl_arrlen(window->mouse.button); i++) {
        window->mouse.button[i].first = false;
    }
    glfwPollEvents();
}

b8 window_is_open(const Window* window) {
    return window->is_open;
}

void window_swap_buffers(Window* window) {
    glfwSwapBuffers(window->handle);
}

void window_make_current(Window* window) {
    glfwMakeContextCurrent(window->handle);
}

WDL_Ivec2 window_get_size(const Window* window) {
    return window->size;
}

void* window_get_user_data(const Window* window) {
    return window->user_data;
}

b8 window_key_down(const Window* window, Key key) {
    return window->keyboard[key].down;
}

b8 window_key_up(const Window* window, Key key) {
    return !window->keyboard[key].down;
}

b8 window_key_pressed(const Window* window, Key key) {
    return window->keyboard[key].down && window->keyboard[key].first;
}

b8 window_key_released(const Window* window, Key key) {
    return !window->keyboard[key].down && window->keyboard[key].first;
}

WDL_Vec2 window_mouse_pos(const Window* window) {
    return window->mouse.pos;
}

b8 window_mouse_button_down(const Window* window, MouseButton button) {
    return window->mouse.button[button].down;
}

b8 window_mouse_button_up(const Window* window, MouseButton button) {
    return !window->mouse.button[button].down;
}

b8 window_mouse_button_pressed(const Window* window, MouseButton button) {
    return window->mouse.button[button].down && window->mouse.button[button].first;
}

b8 window_mouse_button_released(const Window* window, MouseButton button) {
    return !window->mouse.button[button].down && window->mouse.button[button].first;
}
