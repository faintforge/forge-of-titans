#include "renderer.h"
#include "graphics.h"

// -- Render pass --------------------------------------------------------------

RenderPass render_pass_new(RenderPassDesc desc) {
    RenderPass pass = {
        .desc = desc,
    };

    // Pass is targeting the swapchain.
    if (desc.target_count == 0) {
        pass.targets_swapchain = true;
        return pass;
    }

    pass.fb = gfx_framebuffer_new();
    for (u8 i = 0; i < desc.target_count; i++) {
        gfx_framebuffer_attach(pass.fb, desc.targets[i], i);
    }

    return pass;
}

void render_pass_execute(RenderPass pass) {
    RenderPassDesc desc = pass.desc;
    gfx_viewport(desc.viewport.width, desc.viewport.height);

    if (!pass.targets_swapchain) {
        gfx_framebuffer_bind(pass.fb);
    } else {
        gfx_framebuffer_unbind();
    }

    desc.execute(desc.inputs, desc.input_count, desc.user_data);

    if (!pass.targets_swapchain) {
        gfx_framebuffer_unbind();
    }
}

// -- Render pipeline ----------------------------------------------------------

void render_pipeline_add_pass(RenderPipeline* pipeline, RenderPass pass) {
    pipeline->passes[pipeline->pass_count++] = pass;
}

void render_pipeline_execute(const RenderPipeline* pipeline) {
    for (u8 i = 0; i < pipeline->pass_count; i++) {
        render_pass_execute(pipeline->passes[i]);
    }
}
