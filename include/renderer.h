#ifndef RENDERER_H
#define RENDERER_H

#include "graphics.h"

// -- Render pass --------------------------------------------------------------

typedef struct RenderPass RenderPass;

typedef struct RenderPassDesc RenderPassDesc;
struct RenderPassDesc {
    void (*execute)(const GfxTexture* inputs, u8 input_count, void* user_data);
    void* user_data;

    void (*resize)(RenderPass* pass);
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

#endif // RENDERER_H
