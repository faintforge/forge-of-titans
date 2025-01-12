#ifndef RENDERER_H
#define RENDERER_H

#include "graphics.h"

typedef struct RenderPass RenderPass;

typedef struct RenderPassDesc RenderPassDesc;
struct RenderPassDesc {
    void (*run)(const GfxTexture* inputs, u8 input_count, void* user_data);
    void* user_data;

    void (*resize)(RenderPass* pass);
    b8 screen_size_dependant;

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
extern void       render_pass_run(RenderPass pass);

#endif // RENDERER_H
