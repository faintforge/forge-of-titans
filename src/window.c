#include "window.h"
#include "waddle.h"

#include <GLFW/glfw3.h>

struct Window {
    GLFWwindow* handle;
    b8 is_open;
    u32 width;
    u32 height;
    b8 vsync;

    ResizeCallback resize_cb;
};

static void close_cb(GLFWwindow* handle) {
    Window* window = glfwGetWindowUserPointer(handle);
    window->is_open = false;
}

static void internal_resize_cb(GLFWwindow* handle, i32 width, i32 height) {
    Window* window = glfwGetWindowUserPointer(handle);
    window->width = width;
    window->height = height;
    if (window->resize_cb != NULL) {
        window->resize_cb(window, width, height);
    }
}

Window* window_create(WDL_Arena* arena, WindowDesc desc) {
    if (!glfwInit()) {
        return NULL;
    }

    Window* window = wdl_arena_push(arena, sizeof(Window));

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable);
    *window = (Window) {
        .handle = glfwCreateWindow(desc.width,
                desc.height,
                desc.title,
                NULL,
                NULL),
        .is_open = true,
        .width = desc.width,
        .height = desc.height,
        .vsync = desc.vsync,
        .resize_cb = desc.resize_cb,
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
