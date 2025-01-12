#include "renderer.h"
#include "graphics.h"

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

void render_pass_run(RenderPass pass) {
    if (!pass.targets_swapchain) {
        gfx_framebuffer_bind(pass.fb);
    } else {
        gfx_framebuffer_unbind();
    }

    RenderPassDesc desc = pass.desc;
    desc.run(desc.inputs, desc.input_count, desc.user_data);

    if (!pass.targets_swapchain) {
        gfx_framebuffer_unbind();
    }
}
