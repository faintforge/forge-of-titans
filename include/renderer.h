#ifndef RENDERER_H
#define RENDERER_H

#include "graphics.h"

// -- Render pass --------------------------------------------------------------

typedef struct RenderPass RenderPass;

typedef struct RenderPassDesc RenderPassDesc;
struct RenderPassDesc {
    void (*execute)(const GfxTexture* inputs, u8 input_count, void* user_data);
    void* user_data;

    void (*resize)(RenderPassDesc* pass, WDL_Ivec2 size);
    b8 screen_size_dependant;

    WDL_Ivec2 viewport;

    // If there's no targets specificed then it's assumed the pass is targeting
    // the swapchain.
    GfxTexture targets[8];
    u8 target_count;

    GfxTexture inputs[8];
    u8 input_count;
};

struct RenderPass {
    RenderPassDesc desc;
    GfxFramebuffer fb;
    b8 targets_swapchain;
};

extern RenderPass render_pass_new(RenderPassDesc desc);
extern void       render_pass_execute(RenderPass pass);

// -- Render pipeline ----------------------------------------------------------

typedef struct RenderPipeline RenderPipeline;
struct RenderPipeline {
    RenderPass passes[32];
    u8 pass_count;
};

extern void render_pipeline_add_pass(RenderPipeline* pipeline, RenderPass pass);
extern void render_pipeline_execute(const RenderPipeline* pipeline);
extern void render_pipeline_resize(RenderPipeline* pipeline, WDL_Ivec2 size);

// -- Camera -------------------------------------------------------------------

typedef struct Camera Camera;
struct Camera {
    WDL_Ivec2 screen_size;
    WDL_Vec2 pos;
    f32 zoom;
};

extern WDL_Mat4 camera_proj(Camera cam);
extern WDL_Mat4 camera_proj_inv(Camera cam);

// -- Batch renderer -----------------------------------------------------------

typedef struct BatchRenderer BatchRenderer;

typedef struct Quad Quad;
struct Quad {
    WDL_Vec2 pos;
    WDL_Vec2 size;
    f32 rotation;
    GfxColor color;
    GfxTexture texture;
    WDL_Vec2 pivot;
};

extern BatchRenderer* batch_renderer_new(WDL_Arena* arena, u32 max_quad_count);
extern void batch_begin(BatchRenderer* br, GfxShader shader);
extern void batch_end(BatchRenderer* br);
extern void draw_quad(BatchRenderer* br, Quad quad, Camera cam);
// uvs[0] = Top left
// uvs[1] = Bottom right
extern void draw_quad_atlas(BatchRenderer* br, Quad quad, WDL_Vec2 uvs[2], Camera cam);

// -- Debug --------------------------------------------------------------------

typedef struct DebugDrawCmd DebugDrawCmd;
struct DebugDrawCmd {
    DebugDrawCmd* next;
    Quad quad;
    Camera cam;
};

typedef struct DebugCtx DebugCtx;
struct DebugCtx {
    WDL_Arena* arena;

    DebugCtx* _next;
    DebugDrawCmd* _first_cmd;
    DebugDrawCmd* _last_cmd;
};

extern void debug_ctx_push(DebugCtx* ctx);
extern void debug_ctx_pop(void);
extern void debug_ctx_execute(const DebugCtx* ctx, BatchRenderer* br);
extern void debug_ctx_reset(DebugCtx* ctx);

extern void debug_draw_quad(Quad quad, Camera cam);
extern void debug_draw_quad_outline(Quad quad, Camera cam);

extern void debug_draw_line(WDL_Vec2 a, WDL_Vec2 b, GfxColor color, Camera cam);
extern void debug_draw_line_angle(WDL_Vec2 pos, f32 angle, f32 length, GfxColor color, Camera cam);

#endif // RENDERER_H
