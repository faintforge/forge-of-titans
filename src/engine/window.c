#include "engine/window.h"
#include "waddle.h"

#include <GLFW/glfw3.h>

struct Window {
    GLFWwindow* handle;
    b8 is_open;
    WDL_Ivec2 size;
    b8 vsync;
    void* user_data;

    ResizeCallback resize_cb;
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
    glfwSetWindowUserPointer(window->handle, window);
    glfwSetWindowCloseCallback(window->handle, close_cb);
    glfwSetFramebufferSizeCallback(window->handle, internal_resize_cb);

    glfwMakeContextCurrent(window->handle);
    glfwSwapInterval(desc.vsync);
    glfwMakeContextCurrent(NULL);

    return window;
}

void window_destroy(Window* window) {
    glfwDestroyWindow(window->handle);
    glfwTerminate();
}

void window_poll_events(void) {
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
