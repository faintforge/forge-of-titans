#include "graphics.h"
#include "waddle.h"

#include <math.h>

#include <glad/gl.h>

#define ASSERT(EXPR, MSG, ...) do { \
    if (!(EXPR)) { \
        wdl_fatal(MSG, ##__VA_ARGS__); \
        *(volatile u8*) 0 = 0; \
    } \
} while (0)

// -- Internal structures ------------------------------------------------------

//
// Resource pool
// NOTE: Having the resource pool be generic makes it so type information is
// lost. Using the wrong pool for the resource won't report as an error. Perhaps
// there's some clever macro thing to solve this and make it more robust.
//

typedef struct PoolNode PoolNode;
struct PoolNode {
    PoolNode* next;
    void* data;
};

typedef struct ResourcePool ResourcePool;
struct ResourcePool {
    WDL_Arena* arena;
    u32 resource_size;
    PoolNode* nodes;
};

#define POOL_ITER(POOL, NAME) for (PoolNode* NAME = (POOL).nodes; NAME != NULL; NAME = NAME->next)

static ResourcePool resource_pool_init(WDL_Arena* arena, u32 resource_size) {
    ResourcePool pool = {
        .arena = arena,
        .resource_size = resource_size,
    };
    return pool;
}

static PoolNode* resource_pool_aquire(ResourcePool* pool) {
    PoolNode* node = wdl_arena_push_no_zero(pool->arena, sizeof(PoolNode));
    *node = (PoolNode) {
        .data = wdl_arena_push(pool->arena, pool->resource_size),
    };
    return node;
}

static inline void* resource_pool_get_data(PoolNode* node) { return node->data; }

// -- State --------------------------------------------------------------------

typedef struct InternalBuffer InternalBuffer;
struct InternalBuffer {
    u32 gl_handle;
    u64 size;
};

typedef struct InternalVertexArray InternalVertexArray;
struct InternalVertexArray {
    u32 gl_handle;
    GfxBuffer index_buffer;
};

typedef struct InternalShader InternalShader;
struct InternalShader {
    u32 gl_handle;
};

typedef struct InternalTexture InternalTexture;
struct InternalTexture {
    u32 gl_handle;
    WDL_Ivec2 size;
};

typedef struct InternalFramebuffer InternalFramebuffer;
struct InternalFramebuffer {
    u32 gl_handle;
};

typedef struct GraphicsState GraphicsState;
struct GraphicsState {
    WDL_Arena* arena;
    WDL_Lib* lib_gl;

    ResourcePool buffer_pool;
    ResourcePool vertex_array_pool;
    ResourcePool shader_pool;
    ResourcePool texture_pool;
    ResourcePool framebuffer_pool;
};

static GraphicsState state = {0};

b8 gfx_init(void) {
    WDL_Arena* arena = wdl_arena_create();
    state = (GraphicsState) {
        .arena = arena,
        .lib_gl = wdl_lib_load(arena, "libGL.so"),

        .buffer_pool = resource_pool_init(arena, sizeof(InternalBuffer)),
        .vertex_array_pool = resource_pool_init(arena, sizeof(InternalVertexArray)),
        .shader_pool = resource_pool_init(arena, sizeof(InternalShader)),
        .texture_pool = resource_pool_init(arena, sizeof(InternalTexture)),
        .framebuffer_pool = resource_pool_init(arena, sizeof(InternalFramebuffer)),
    };

    if (!gladLoadGLUserPtr((GLADuserptrloadfunc) wdl_lib_func, state.lib_gl)) {
        return false;
    }

    return true;
}

void gfx_termiante(void) {
    POOL_ITER(state.buffer_pool, node) {
        InternalBuffer* buf = node->data;
        glDeleteBuffers(1, &buf->gl_handle);
    }

    POOL_ITER(state.vertex_array_pool, node) {
        InternalVertexArray* va = node->data;
        glDeleteVertexArrays(1, &va->gl_handle);
    }

    POOL_ITER(state.shader_pool, node) {
        InternalShader* shader = node->data;
        glDeleteProgram(shader->gl_handle);
    }

    POOL_ITER(state.texture_pool, node) {
        InternalTexture* texture = node->data;
        glDeleteTextures(1, &texture->gl_handle);
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
    PoolNode* node = resource_pool_aquire(&state.buffer_pool);
    InternalBuffer* internal_buffer = node->data;
    glGenBuffers(1, &internal_buffer->gl_handle);
    GfxBuffer buffer = { .handle = node };
    gfx_buffer_resize(buffer, desc);
    return buffer;
}

void gfx_buffer_resize(GfxBuffer buffer, GfxBufferDesc desc) {
    ASSERT(!gfx_buffer_is_null(buffer), "Cannot resize a NULL buffer!");

    InternalBuffer* internal = resource_pool_get_data(buffer.handle);
    internal->size = desc.size;

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

    glBindBuffer(BUFFER_OP_TARGET, internal->gl_handle);
    glBufferData(BUFFER_OP_TARGET, desc.size, desc.data, gl_usage);
    glBindBuffer(BUFFER_OP_TARGET, 0);
}

void gfx_buffer_subdata(GfxBuffer buffer, const void* data, u32 size, u32 offset) {
    ASSERT(!gfx_buffer_is_null(buffer), "Cannot enter subdata into a NULL buffer!");

    InternalBuffer* internal = resource_pool_get_data(buffer.handle);
    ASSERT(offset + size <= internal->size, "Buffer overflow. Trying to write outside of the buffers capacity. Run 'gfx_buffer_resize()' to change the size.");

    glBindBuffer(BUFFER_OP_TARGET, internal->gl_handle);
    glBufferSubData(BUFFER_OP_TARGET, offset, size, data);
}

b8 gfx_buffer_is_null(GfxBuffer buffer) {
    return buffer.handle == NULL;
}

// -- Vertex array -------------------------------------------------------------

GfxVertexArray gfx_vertex_array_new(GfxVertexArrayDesc desc) {
    ASSERT(!gfx_buffer_is_null(desc.vertex_buffer), "Vertex array must have a vertex buffer!");

    PoolNode* node = resource_pool_aquire(&state.vertex_array_pool);
    InternalVertexArray* internal = node->data;
    glGenVertexArrays(1, &internal->gl_handle);
    internal->index_buffer = desc.index_buffer;
    InternalBuffer* buf_node = resource_pool_get_data(desc.vertex_buffer.handle);

    glBindVertexArray(internal->gl_handle);
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
        wdl_error("Vertex shader compilation error: %s", info_log);
        return GFX_SHADER_NULL;
    }

    u32 f_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_shader, 1, (const char* const*) &fragment_source.data, (const int*) &fragment_source.len);
    glCompileShader(f_shader);
    glGetShaderiv(f_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(f_shader, sizeof(info_log), NULL, info_log);
        wdl_error("Fragment shader compilation error: %s\n", info_log);
        return GFX_SHADER_NULL;
    }

    u32 program = glCreateProgram();
    glAttachShader(program, v_shader);
    glAttachShader(program, f_shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, info_log);
        wdl_error("Shader linking error: %s\n", info_log);
        return GFX_SHADER_NULL;
    }

    glDeleteShader(v_shader);
    glDeleteShader(f_shader);

    PoolNode* node = resource_pool_aquire(&state.shader_pool);
    InternalShader* internal = node->data;
    internal->gl_handle = program;
    return (GfxShader) { .handle = node };
}

void gfx_shader_use(GfxShader shader) {
    ASSERT(!gfx_shader_is_null(shader), "Can't use a NULL shader!");
    InternalShader* internal = resource_pool_get_data(shader.handle);
    glUseProgram(internal->gl_handle);
}

b8 gfx_shader_is_null(GfxShader shader) {
    return shader.handle == NULL;
}

#define uniform_body(shader, name) \
    ASSERT(!gfx_shader_is_null(shader), "Can't set a uniform on a NULL shader."); \
    InternalShader* internal = resource_pool_get_data(shader.handle); \
    glUseProgram(internal->gl_handle); \
    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0); \
    const char* cstr = wdl_str_to_cstr(scratch.arena, name); \
    u32 loc = glGetUniformLocation(internal->gl_handle, cstr); \
    wdl_scratch_end(scratch);

void gfx_shader_uniform_i32(GfxShader shader, WDL_Str name, i32 value) {
    uniform_body(shader, name);
    glUniform1i(loc, value);
}

void gfx_shader_uniform_i32_arr(GfxShader shader, WDL_Str name, const i32* arr, u32 count) {
    uniform_body(shader, name);
    glUniform1iv(loc, count, arr);
}

void gfx_shader_uniform_m4(GfxShader shader, WDL_Str name, WDL_Mat4 value) {
    uniform_body(shader, name);
    glUniformMatrix4fv(loc, 1, false, &value.a.x);
}

void gfx_shader_uniform_m4_arr(GfxShader shader, WDL_Str name, const WDL_Mat4* arr, u32 count) {
    uniform_body(shader, name);
    glUniformMatrix4fv(loc, count, false, (const void*) arr);
}

#undef uniform_body

// -- Texture ------------------------------------------------------------------

GfxTexture gfx_texture_new(GfxTextureDesc desc) {
    PoolNode* node = resource_pool_aquire(&state.texture_pool);
    InternalTexture* internal = node->data;
    glGenTextures(1, &internal->gl_handle);
    GfxTexture texture = { .handle = node };
    gfx_texture_resize(texture, desc);
    return texture;
}

void gfx_texture_bind(GfxTexture texture, u32 slot) {
    InternalTexture* internal = resource_pool_get_data(texture.handle);
    glBindTextureUnit(slot, internal->gl_handle);
}

static void _texture_format_to_gl_format(GfxTextureFormat format, u32* gl_internal_format, u32* gl_format, u32* gl_type) {
    switch (format) {
        case GFX_TEXTURE_FORMAT_R_U8:
            *gl_internal_format = GL_R8;
            *gl_format = GL_RED;
            break;
        case GFX_TEXTURE_FORMAT_RG_U8:
            *gl_internal_format = GL_RG8;
            *gl_format = GL_RG;
            break;
        case GFX_TEXTURE_FORMAT_RGB_U8:
            *gl_internal_format = GL_RGB8;
            *gl_format = GL_RGB;
            break;
        case GFX_TEXTURE_FORMAT_RGBA_U8:
            *gl_internal_format = GL_RGBA8;
            *gl_format = GL_RGBA;
            break;

        case GFX_TEXTURE_FORMAT_R_F16:
            *gl_internal_format = GL_R16F;
            *gl_format = GL_RED;
            break;
        case GFX_TEXTURE_FORMAT_RG_F16:
            *gl_internal_format = GL_RG16F;
            *gl_format = GL_RG;
            break;
        case GFX_TEXTURE_FORMAT_RGB_F16:
            *gl_internal_format = GL_RGB16F;
            *gl_format = GL_RGB;
            break;
        case GFX_TEXTURE_FORMAT_RGBA_F16:
            *gl_internal_format = GL_RGBA16F;
            *gl_format = GL_RGBA;
            break;

        case GFX_TEXTURE_FORMAT_R_F32:
            *gl_internal_format = GL_R32F;
            *gl_format = GL_RED;
            break;
        case GFX_TEXTURE_FORMAT_RG_F32:
            *gl_internal_format = GL_RG32F;
            *gl_format = GL_RG;
            break;
        case GFX_TEXTURE_FORMAT_RGB_F32:
            *gl_internal_format = GL_RGB32F;
            *gl_format = GL_RGB;
            break;
        case GFX_TEXTURE_FORMAT_RGBA_F32:
            *gl_internal_format = GL_RGBA32F;
            *gl_format = GL_RGBA;
            break;
    }

    switch (format) {
        case GFX_TEXTURE_FORMAT_R_U8:
        case GFX_TEXTURE_FORMAT_RG_U8:
        case GFX_TEXTURE_FORMAT_RGB_U8:
        case GFX_TEXTURE_FORMAT_RGBA_U8:
            *gl_type = GL_UNSIGNED_BYTE;
            break;

        case GFX_TEXTURE_FORMAT_R_F16:
        case GFX_TEXTURE_FORMAT_RG_F16:
        case GFX_TEXTURE_FORMAT_RGB_F16:
        case GFX_TEXTURE_FORMAT_RGBA_F16:
            *gl_type = GL_HALF_FLOAT;
            break;

        case GFX_TEXTURE_FORMAT_R_F32:
        case GFX_TEXTURE_FORMAT_RG_F32:
        case GFX_TEXTURE_FORMAT_RGB_F32:
        case GFX_TEXTURE_FORMAT_RGBA_F32:
            *gl_type = GL_FLOAT;
            break;
    }
}

void gfx_texture_resize(GfxTexture texture, GfxTextureDesc desc) {
    InternalTexture* internal = resource_pool_get_data(texture.handle);
    // Default to 4 since that's OpenGL's default.
    if (desc.alignment == 0) {
        desc.alignment = 4;
    }

    internal->size = desc.size;
    glPixelStorei(GL_UNPACK_ALIGNMENT, desc.alignment);

    u32 gl_internal_format;
    u32 gl_format;
    u32 gl_type;
    _texture_format_to_gl_format(desc.format, &gl_internal_format, &gl_format, &gl_type);

    u32 gl_sampler;
    switch (desc.sampler) {
        case GFX_TEXTURE_SAMPLER_LINEAR:
            gl_sampler = GL_LINEAR;
            break;
        case GFX_TEXTURE_SAMPLER_NEAREST:
            gl_sampler = GL_NEAREST;
            break;
    }

    glBindTexture(GL_TEXTURE_2D, internal->gl_handle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_sampler);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_sampler);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            gl_internal_format,
            desc.size.x,
            desc.size.y,
            0,
            gl_format,
            gl_type,
            desc.data);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void gfx_texture_subdata(GfxTexture texture, GfxTextureSubDataDesc desc) {
    InternalTexture* internal = resource_pool_get_data(texture.handle);
    // Default to 4 since that's OpenGL's default.
    if (desc.alignment == 0) {
        desc.alignment = 4;
    }
    glBindTexture(GL_TEXTURE_2D, internal->gl_handle);
    glPixelStorei(GL_UNPACK_ALIGNMENT, desc.alignment);
    u32 gl_internal_format;
    u32 gl_format;
    u32 gl_type;
    _texture_format_to_gl_format(desc.format, &gl_internal_format, &gl_format, &gl_type);
    glTexSubImage2D(GL_TEXTURE_2D, 0, desc.pos.x, desc.pos.y, desc.size.x, desc.size.y, gl_format, gl_type, desc.data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

WDL_Ivec2 gfx_texture_get_size(GfxTexture texture) {
    InternalTexture* internal = resource_pool_get_data(texture.handle);
    return internal->size;
}

b8 gfx_texture_is_null(GfxTexture texture) {
    return texture.handle == NULL;
}

// -- Framebuffer --------------------------------------------------------------

GfxFramebuffer gfx_framebuffer_new(void) {
    PoolNode* node = resource_pool_aquire(&state.framebuffer_pool);
    InternalFramebuffer* internal = node->data;
    glGenFramebuffers(1, &internal->gl_handle);
    return (GfxFramebuffer) { .handle = node };
}

void gfx_framebuffer_attach(GfxFramebuffer framebuffer, GfxTexture texture, u32 slot) {
    InternalFramebuffer* internal = resource_pool_get_data(framebuffer.handle);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->gl_handle);
    InternalTexture* internal_texture = resource_pool_get_data(texture.handle);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + slot, GL_TEXTURE_2D, internal_texture->gl_handle, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void gfx_framebuffer_bind(GfxFramebuffer framebuffer) {
    InternalFramebuffer* internal = resource_pool_get_data(framebuffer.handle);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->gl_handle);
}

void gfx_framebuffer_unbind(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// -- Drawing ------------------------------------------------------------------

void gfx_clear(GfxColor color) {
    glClearColor(gfx_color_arg(color));
    glClear(GL_COLOR_BUFFER_BIT);
}

void gfx_draw(GfxVertexArray vertex_array, u32 vertex_count, u32 first_vertex) {
    ASSERT(!gfx_vertex_array_is_null(vertex_array), "No vertex buffer provided at draw!");
    InternalVertexArray* node = vertex_array.handle;

    glBindVertexArray(node->gl_handle);
    glDrawArrays(GL_TRIANGLES, first_vertex, vertex_count);
    glBindVertexArray(0);
}

void gfx_draw_indexed(GfxVertexArray vertex_array, u32 index_count, u32 first_index) {
    ASSERT(!gfx_vertex_array_is_null(vertex_array), "No vertex buffer provided at draw!");
    InternalVertexArray* internal_va = resource_pool_get_data(vertex_array.handle);
    ASSERT(!gfx_buffer_is_null(internal_va->index_buffer), "Can't draw indexed without an index buffer bound to vertex array!");
    InternalBuffer* index_buffer = resource_pool_get_data(internal_va->index_buffer.handle);

    glBindVertexArray(internal_va->gl_handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->gl_handle);

    glDrawElements(GL_TRIANGLES,
            index_count,
            GL_UNSIGNED_INT,
            (const void*) (first_index * sizeof(u32)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void gfx_viewport(WDL_Ivec2 size) {
    glViewport(0, 0, size.x, size.y);
}
