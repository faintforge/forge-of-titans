#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "waddle.h"

extern b8   gfx_init(void);
extern void gfx_termiante(void);

// -- Color --------------------------------------------------------------------

typedef struct GfxColor GfxColor;
struct GfxColor {
    f32 r, g, b, a;
};

extern GfxColor gfx_color_rgba_f(f32 r, f32 g, f32 b, f32 a);
extern GfxColor gfx_color_rgba_i(u8 r, u8 g, u8 b, u8 a);
extern GfxColor gfx_color_rgba_hex(u32 hex);
extern GfxColor gfx_color_rgb_f(f32 r, f32 g, f32 b);
extern GfxColor gfx_color_rgb_i(u8 r, u8 g, u8 b);

extern GfxColor gfx_color_rgb_hex(u32 hex);
extern GfxColor gfx_color_hsl(f32 hue, f32 saturation, f32 lightness);
extern GfxColor gfx_color_hsv(f32 hue, f32 saturation, f32 value);

#define gfx_color_arg(color) (color).r, (color).g, (color).b, (color).a

#define GFX_COLOR_WHITE ((GfxColor) {1.0f, 1.0f, 1.0f, 1.0f})
#define GFX_COLOR_BLACK ((GfxColor) {0.0f, 0.0f, 0.0f, 1.0f})
#define GFX_COLOR_RED ((GfxColor) {1.0f, 0.0f, 0.0f, 1.0f})
#define GFX_COLOR_GREEN ((GfxColor) {0.0f, 1.0f, 0.0f, 1.0f})
#define GFX_COLOR_BLUE ((GfxColor) {0.0f, 0.0f, 1.0f, 1.0f})
#define GFX_COLOR_TRANSPARENT ((GfxColor) {0.0f, 0.0f, 0.0f, 0.0f})

// -- Buffer -------------------------------------------------------------------

typedef struct GfxBuffer GfxBuffer;
struct GfxBuffer {
    void* handle;
};

typedef enum GfxBufferUsage {
    GFX_BUFFER_USAGE_STATIC,
    GFX_BUFFER_USAGE_DYNAMIC,
    GFX_BUFFER_USAGE_STREAM,
} GfxBufferUsage;

typedef struct GfxBufferDesc GfxBufferDesc;
struct GfxBufferDesc {
    const void* data;
    u64 size;
    GfxBufferUsage usage;
};

#define GFX_BUFFER_NULL ((GfxBuffer) { NULL })

extern GfxBuffer gfx_buffer_new(GfxBufferDesc desc);
extern b8        gfx_buffer_is_null(GfxBuffer buffer);

// -- Vertex array -------------------------------------------------------------

typedef struct GfxVertexArray GfxVertexArray;
struct GfxVertexArray {
    void* handle;
};

typedef struct GfxVertexAttrib GfxVertexAttrib;
struct GfxVertexAttrib {
    u32 count;
    u64 offset;
};

typedef struct GfxVertexLayout GfxVertexLayout;
struct GfxVertexLayout {
    u64 size;
    GfxVertexAttrib attribs[32];
    u32 attrib_count;
};

typedef struct GfxVertexArrayDesc GfxVertexArrayDesc;
struct GfxVertexArrayDesc {
    GfxBuffer vertex_buffer;
    GfxVertexLayout layout;
    GfxBuffer index_buffer;
};

#define GFX_VERTEX_ARRAY_NULL ((GfxVertexArray) { NULL })

extern GfxVertexArray gfx_vertex_array_new(GfxVertexArrayDesc desc);
extern b8             gfx_vertex_array_is_null(GfxVertexArray vertex_array);

// -- Shader -------------------------------------------------------------------

typedef struct GfxShader GfxShader;
struct GfxShader {
    void* handle;
};

#define GFX_SHADER_NULL ((GfxShader) { NULL })

extern GfxShader gfx_shader_new(WDL_Str vertex_source, WDL_Str fragment_source);
extern void      gfx_shader_use(GfxShader shader);
extern b8        gfx_shader_is_null(GfxShader shader);

// -- Drawing ------------------------------------------------------------------

extern void gfx_clear(GfxColor color);
extern void gfx_draw(GfxVertexArray vertex_array, u32 vertex_count, u32 first_vertex);
extern void gfx_draw_indexed(GfxVertexArray vertex_array, u32 index_count, u32 first_index);

#endif // GRAPHICS_H
