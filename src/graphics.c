#include "graphics.h"
#include "waddle.h"

#include <math.h>

#include <glad/gl.h>

#define FOR_LL(T, LIST, NAME) for (T* NAME = (LIST); NAME != NULL; NAME = NAME->next)
#define ASSERT(EXPR, MSG, ...) do { \
    if (!(EXPR)) { \
        WDL_FATAL(MSG, ##__VA_ARGS__); \
        *(volatile u8*) 0 = 0; \
    } \
} while (0)

// -- Internal structures ------------------------------------------------------
// TODO: Find a way to abstract the pool structure to reduce duplicate code.

//
// Buffer pool
//

typedef struct BufferNode BufferNode;
struct BufferNode {
    BufferNode* next;
    u32 gl_handle;
    u64 size;
};

typedef struct BufferPool BufferPool;
struct BufferPool {
    WDL_Arena* arena;
    BufferNode* nodes;
};

static BufferPool buffer_pool_init(WDL_Arena* arena) {
    BufferPool buff_pool = {
        .arena = arena,
    };
    return buff_pool;
}

static BufferNode* buffer_pool_get_handle(BufferPool* pool) {
    BufferNode* node = wdl_arena_push_no_zero(pool->arena, sizeof(BufferNode));
    node->next = pool->nodes;
    pool->nodes = node;
    glCreateBuffers(1, &node->gl_handle);
    return node;
}

//
// Vertex array pool
//

typedef struct VertexArrayNode VertexArrayNode;
struct VertexArrayNode {
    VertexArrayNode* next;
    u32 gl_handle;
    GfxBuffer index_buffer;
};

typedef struct VertexArrayPool VertexArrayPool;
struct VertexArrayPool {
    WDL_Arena* arena;
    VertexArrayNode* nodes;
};

static VertexArrayPool vertex_array_pool_init(WDL_Arena* arena) {
    VertexArrayPool buff_pool = {
        .arena = arena,
    };
    return buff_pool;
}

static VertexArrayNode* vertex_array_pool_get_handle(VertexArrayPool* pool) {
    VertexArrayNode* node = wdl_arena_push_no_zero(pool->arena, sizeof(VertexArrayNode));
    node->next = pool->nodes;
    pool->nodes = node;
    glCreateVertexArrays(1, &node->gl_handle);
    return node;
}

//
// Shader Pool
//

typedef struct ShaderNode ShaderNode;
struct ShaderNode {
    ShaderNode* next;
    u32 gl_handle;
};

typedef struct ShaderPool ShaderPool;
struct ShaderPool {
    WDL_Arena* arena;
    ShaderNode* nodes;
};

static ShaderPool shader_pool_init(WDL_Arena* arena) {
    ShaderPool buff_pool = {
        .arena = arena,
    };
    return buff_pool;
}

static ShaderNode* shader_pool_get_handle(ShaderPool* pool) {
    ShaderNode* node = wdl_arena_push_no_zero(pool->arena, sizeof(ShaderNode));
    node->next = pool->nodes;
    pool->nodes = node;
    return node;
}

typedef struct GraphicsState GraphicsState;
struct GraphicsState {
    WDL_Arena* arena;
    WDL_Lib* lib_gl;

    BufferPool buffer_pool;
    VertexArrayPool vertex_array_pool;
    ShaderPool shader_pool;
};

static GraphicsState state = {0};

b8 gfx_init(void) {
    WDL_Arena* arena = wdl_arena_create();
    state = (GraphicsState) {
        .arena = arena,
        .lib_gl = wdl_lib_load(arena, "libGL.so"),

        .buffer_pool = buffer_pool_init(arena),
        .vertex_array_pool = vertex_array_pool_init(arena),
        .shader_pool = shader_pool_init(arena),
    };

    if (!gladLoadGLUserPtr((GLADuserptrloadfunc) wdl_lib_func, state.lib_gl)) {
        return false;
    }

    return true;
}

void gfx_termiante(void) {
    FOR_LL(BufferNode, state.buffer_pool.nodes, curr) {
        glDeleteBuffers(1, &curr->gl_handle);
    }

    FOR_LL(VertexArrayNode, state.vertex_array_pool.nodes, curr) {
        glDeleteVertexArrays(1, &curr->gl_handle);
    }

    FOR_LL(ShaderNode, state.shader_pool.nodes, curr) {
        glDeleteProgram(curr->gl_handle);
    }

    wdl_lib_unload(state.lib_gl);
    wdl_arena_destroy(state.arena);
}

// -- Color --------------------------------------------------------------------

GfxColor gfx_color_rgba_f(f32 r, f32 g, f32 b, f32 a) {
    return (GfxColor) {r, g, b, a};
}

GfxColor gfx_color_rgba_i(u8 r, u8 g, u8 b, u8 a) {
    return (GfxColor) {r/255.0f, g/255.0f, b/255.0f, a/255.0f};
}

GfxColor gfx_color_rgba_hex(u32 hex) {
    return (GfxColor) {
        .r = (f32) (hex >> 8 * 3 & 0xff) / 0xff,
        .g = (f32) (hex >> 8 * 2 & 0xff) / 0xff,
        .b = (f32) (hex >> 8 * 1 & 0xff) / 0xff,
        .a = (f32) (hex >> 8 * 0 & 0xff) / 0xff,
    };
}

GfxColor gfx_color_rgb_f(f32 r, f32 g, f32 b) {
    return (GfxColor) {r, g, b, 1.0f};
}

GfxColor gfx_color_rgb_i(u8 r, u8 g, u8 b) {
    return (GfxColor) {r/255.0f, g/255.0f, b/255.0f, 1.0f};
}

GfxColor gfx_color_rgb_hex(u32 hex) {
    return (GfxColor) {
        .r = (f32) (hex >> 8 * 2 & 0xff) / 0xff,
        .g = (f32) (hex >> 8 * 1 & 0xff) / 0xff,
        .b = (f32) (hex >> 8 * 0 & 0xff) / 0xff,
        .a = 1.0f,
    };
}

GfxColor gfx_color_hsl(f32 hue, f32 saturation, f32 lightness) {
    // https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB
    GfxColor color = {0};
    f32 chroma = (1 - fabsf(2 * lightness - 1)) * saturation;
    f32 hue_prime = fabsf(fmodf(hue, 360.0f)) / 60.0f;
    f32 x = chroma * (1.0f - fabsf(fmodf(hue_prime, 2.0f) - 1.0f));
    if (hue_prime < 1.0f) { color = (GfxColor) { chroma, x, 0.0f, 1.0f, }; }
    else if (hue_prime < 2.0f) { color = (GfxColor) { x, chroma, 0.0f, 1.0f, }; }
    else if (hue_prime < 3.0f) { color = (GfxColor) { 0.0f, chroma, x, 1.0f, }; }
    else if (hue_prime < 4.0f) { color = (GfxColor) { 0.0f, x, chroma, 1.0f, }; }
    else if (hue_prime < 5.0f) { color = (GfxColor) { x, 0.0f, chroma, 1.0f, }; }
    else if (hue_prime < 6.0f) { color = (GfxColor) { chroma, 0.0f, x, 1.0f, }; }
    f32 m = lightness-chroma / 2.0f;
    color.r += m;
    color.g += m;
    color.b += m;
    return color;
}

GfxColor gfx_color_hsv(f32 hue, f32 saturation, f32 value) {
    // https://en.wikipedia.org/wiki/HSL_and_HSV#HSV_to_RGB
    GfxColor color = {0};
    f32 chroma = value * saturation;
    f32 hue_prime = fabsf(fmodf(hue, 360.0f)) / 60.0f;
    f32 x = chroma * (1.0f - fabsf(fmodf(hue_prime, 2.0f) - 1.0f));
    if (hue_prime < 1.0f) { color = (GfxColor) { chroma, x, 0.0f, 1.0f, }; }
    else if (hue_prime < 2.0f) { color = (GfxColor) { x, chroma, 0.0f, 1.0f, }; }
    else if (hue_prime < 3.0f) { color = (GfxColor) { 0.0f, chroma, x, 1.0f, }; }
    else if (hue_prime < 4.0f) { color = (GfxColor) { 0.0f, x, chroma, 1.0f, }; }
    else if (hue_prime < 5.0f) { color = (GfxColor) { x, 0.0f, chroma, 1.0f, }; }
    else if (hue_prime < 6.0f) { color = (GfxColor) { chroma, 0.0f, x, 1.0f, }; }
    f32 m = value - chroma;
    color.r += m;
    color.g += m;
    color.b += m;
    return color;
}

// -- Buffer -------------------------------------------------------------------

#define BUFFER_OP_TARGET GL_ARRAY_BUFFER

GfxBuffer gfx_buffer_new(GfxBufferDesc desc) {
    BufferNode* node = buffer_pool_get_handle(&state.buffer_pool);
    GfxBuffer buffer = { .handle = node };
    gfx_buffer_resize(buffer, desc);
    return buffer;
}

void gfx_buffer_resize(GfxBuffer buffer, GfxBufferDesc desc) {
    ASSERT(!gfx_buffer_is_null(buffer), "Cannot resize a NULL buffer!");

    BufferNode* node = buffer.handle;
    node->size = desc.size;

    GLenum gl_usage;
    switch (desc.usage) {
        case GFX_BUFFER_USAGE_STATIC:
            gl_usage = GL_STATIC_DRAW;
            break;
        case GFX_BUFFER_USAGE_DYNAMIC:
            gl_usage = GL_DYNAMIC_DRAW;
            break;
        case GFX_BUFFER_USAGE_STREAM:
            gl_usage = GL_STREAM_DRAW;
            break;
    }

    glBindBuffer(BUFFER_OP_TARGET, node->gl_handle);
    glBufferData(BUFFER_OP_TARGET, desc.size, desc.data, gl_usage);
    glBindBuffer(BUFFER_OP_TARGET, 0);
}

void gfx_buffer_subdata(GfxBuffer buffer, const void* data, u32 size, u32 offset) {
    ASSERT(!gfx_buffer_is_null(buffer), "Cannot enter subdata into a NULL buffer!");

    BufferNode* node = buffer.handle;
    ASSERT(offset + size <= node->size, "Buffer overflow. Trying to write outside of the buffers capacity. Run 'gfx_buffer_resize()' to change the size.");

    glBindBuffer(BUFFER_OP_TARGET, node->gl_handle);
    glBufferSubData(BUFFER_OP_TARGET, offset, size, data);
}

b8 gfx_buffer_is_null(GfxBuffer buffer) {
    return buffer.handle == NULL;
}

// -- Vertex array -------------------------------------------------------------

GfxVertexArray gfx_vertex_array_new(GfxVertexArrayDesc desc) {
    ASSERT(!gfx_buffer_is_null(desc.vertex_buffer), "Vertex array must have a vertex buffer!");

    VertexArrayNode* node = vertex_array_pool_get_handle(&state.vertex_array_pool);
    node->index_buffer = desc.index_buffer;
    BufferNode* buf_node = desc.vertex_buffer.handle;

    glBindVertexArray(node->gl_handle);
    glBindBuffer(GL_ARRAY_BUFFER, buf_node->gl_handle);

    GfxVertexLayout layout = desc.layout;
    for (u32 i = 0; i < layout.attrib_count; i++) {
        glVertexAttribPointer(i,
                layout.attribs[i].count,
                GL_FLOAT,
                false,
                layout.size,
                (const void*) (layout.attribs[i].offset));
        glEnableVertexAttribArray(i);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return (GfxVertexArray) { .handle = node };
}

b8 gfx_vertex_array_is_null(GfxVertexArray vertex_array) {
    return vertex_array.handle == NULL;
}

// -- Shader -------------------------------------------------------------------

GfxShader gfx_shader_new(WDL_Str vertex_source, WDL_Str fragment_source) {
    i32 success = 0;
    char info_log[512] = {0};

    u32 v_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v_shader, 1, (const char* const*) &vertex_source.data, (const int*) &vertex_source.len);
    glCompileShader(v_shader);
    glGetShaderiv(v_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(v_shader, sizeof(info_log), NULL, info_log);
        WDL_ERROR("Vertex shader compilation error: %s", info_log);
        return GFX_SHADER_NULL;
    }

    u32 f_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_shader, 1, (const char* const*) &fragment_source.data, (const int*) &fragment_source.len);
    glCompileShader(f_shader);
    glGetShaderiv(f_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(f_shader, sizeof(info_log), NULL, info_log);
        WDL_ERROR("Fragment shader compilation error: %s\n", info_log);
        return GFX_SHADER_NULL;
    }

    u32 program = glCreateProgram();
    glAttachShader(program, v_shader);
    glAttachShader(program, f_shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, info_log);
        WDL_ERROR("Shader linking error: %s\n", info_log);
        return GFX_SHADER_NULL;
    }

    glDeleteShader(v_shader);
    glDeleteShader(f_shader);

    ShaderNode* node = shader_pool_get_handle(&state.shader_pool);
    node->gl_handle = program;
    return (GfxShader) { .handle = node };
}

void gfx_shader_use(GfxShader shader) {
    ASSERT(!gfx_shader_is_null(shader), "Can't use a NULL shader!");
    ShaderNode* node = shader.handle;
    glUseProgram(node->gl_handle);
}

b8 gfx_shader_is_null(GfxShader shader) {
    return shader.handle == NULL;
}

// -- Drawing ------------------------------------------------------------------

void gfx_clear(GfxColor color) {
    glClearColor(gfx_color_arg(color));
    glClear(GL_COLOR_BUFFER_BIT);
}

void gfx_draw(GfxVertexArray vertex_array, u32 vertex_count, u32 first_vertex) {
    ASSERT(!gfx_vertex_array_is_null(vertex_array), "No vertex buffer provided at draw!");
    VertexArrayNode* node = vertex_array.handle;

    glBindVertexArray(node->gl_handle);
    glDrawArrays(GL_TRIANGLES, first_vertex, vertex_count);
    glBindVertexArray(0);
}

void gfx_draw_indexed(GfxVertexArray vertex_array, u32 index_count, u32 first_index) {
    ASSERT(!gfx_vertex_array_is_null(vertex_array), "No vertex buffer provided at draw!");
    VertexArrayNode* node = vertex_array.handle;
    ASSERT(!gfx_buffer_is_null(node->index_buffer), "Can't draw indexed without an index buffer bound to vertex array!");
    BufferNode* buf_node = node->index_buffer.handle;

    glBindVertexArray(node->gl_handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf_node->gl_handle);

    glDrawElements(GL_TRIANGLES,
            index_count,
            GL_UNSIGNED_INT,
            (const void*) (first_index * sizeof(u32)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
