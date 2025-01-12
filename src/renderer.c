#include "renderer.h"
#include "graphics.h"
#include "waddle.h"
#include "utils.h"

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
    gfx_viewport(desc.viewport);

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

void render_pipeline_resize(RenderPipeline* pipeline, WDL_Ivec2 size) {
    for (u8 i = 0; i < pipeline->pass_count; i++) {
        RenderPass* pass = &pipeline->passes[i];
        if (pass->desc.screen_size_dependant) {
            pass->desc.resize(&pass->desc, size);
        }
    }
}

// -- Camera -------------------------------------------------------------------

WDL_Mat4 camera_proj(Camera cam) {
    f32 aspect = (f32) cam.screen_size.x / (f32) cam.screen_size.y;
    f32 zoom = cam.zoom * 0.5f;
    return wdl_m4_ortho_projection(-aspect * zoom, aspect * zoom, zoom, -zoom, 1.0f, -1.0f);
}

WDL_Mat4 camera_proj_inv(Camera cam) {
    f32 aspect = (f32) cam.screen_size.x / (f32) cam.screen_size.y;
    f32 zoom = cam.zoom * 0.5f;
    return wdl_m4_inv_ortho_projection(-aspect * zoom, aspect * zoom, zoom, -zoom, 1.0f, -1.0f);
}

// -- Batch renderer -----------------------------------------------------------

BatchRenderer batch_renderer_new(WDL_Arena* arena, u32 max_quad_count) {
    u64 vertices_size = max_quad_count * 4 * sizeof(Vertex);

    GfxBuffer vertex_buffer = gfx_buffer_new((GfxBufferDesc) {
            .size = vertices_size,
            .data = NULL,
            .usage = GFX_BUFFER_USAGE_DYNAMIC,
        });

    WDL_Scratch scratch = wdl_scratch_begin(&arena, 1);
    u64 indices_size = max_quad_count * 6 * sizeof(u32);
    u32* indices = wdl_arena_push_no_zero(scratch.arena, indices_size);
    u32 j = 0;
    for (u32 i = 0; i < max_quad_count * 6; i += 6) {
        // First triangle
        indices[i + 0] = j + 0;
        indices[i + 1] = j + 1;
        indices[i + 2] = j + 2;
        // Second triangle
        indices[i + 3] = j + 2;
        indices[i + 4] = j + 3;
        indices[i + 5] = j + 1;
        j += 4;
    }
    GfxBuffer index_buffer = gfx_buffer_new((GfxBufferDesc) {
            .size = indices_size,
            .data = indices,
            .usage = GFX_BUFFER_USAGE_STATIC,
        });
    wdl_scratch_end(scratch);

    scratch = wdl_scratch_begin(&arena, 1);
    WDL_Str vertex_source = read_file(scratch.arena, WDL_STR_LIT("assets/shaders/batch.vert.glsl"));
    WDL_Str fragment_source = read_file(scratch.arena, WDL_STR_LIT("assets/shaders/batch.frag.glsl"));
    GfxShader shader = gfx_shader_new(vertex_source, fragment_source);
    wdl_scratch_end(scratch);

    BatchRenderer br = {
        .max_quad_count = max_quad_count,

        .vertices = wdl_arena_push_no_zero(arena, vertices_size),
        .vertex_buffer = vertex_buffer,
        .vertex_array = gfx_vertex_array_new((GfxVertexArrayDesc) {
                .layout = {
                    .size = sizeof(Vertex),
                    .attribs = {
                        [0] = {
                            .count = 2,
                            .offset = WDL_OFFSET(Vertex, pos),
                        },
                        [1] = {
                            .count = 2,
                            .offset = WDL_OFFSET(Vertex, uv),
                        },
                        [2] = {
                            .count = 4,
                            .offset = WDL_OFFSET(Vertex, color),
                        },
                    },
                    .attrib_count = 3,
                },
                .vertex_buffer = vertex_buffer,
                .index_buffer = index_buffer,
            }),
        .shader = shader,
    };
    return br;
}

void batch_begin(BatchRenderer* br) {
    br->curr_quad = 0;
}

void batch_end(BatchRenderer* br) {
    gfx_buffer_subdata(br->vertex_buffer, br->vertices, br->curr_quad * 4 * sizeof(Vertex), 0);
    gfx_shader_use(br->shader);
    gfx_draw_indexed(br->vertex_array, br->curr_quad * 6, 0);
}

void draw_quad(BatchRenderer* br, Quad quad, Camera cam) {
    const WDL_Vec2 vert_pos[4] = {
        wdl_v2(-0.5f, -0.5f),
        wdl_v2( 0.5f, -0.5f),
        wdl_v2(-0.5f,  0.5f),
        wdl_v2( 0.5f,  0.5f),
    };

    const WDL_Vec2 uv[4] = {
        wdl_v2(0.0f, 0.0f),
        wdl_v2(1.0f, 0.0f),
        wdl_v2(0.0f, 1.0f),
        wdl_v2(1.0f, 1.0f),
    };

    // TODO: Cache the projection matrix in the camera.
    WDL_Mat4 proj = camera_proj(cam);
    for (u8 i = 0; i < 4; i++) {
        WDL_Vec2 pos = vert_pos[i];
        pos = wdl_v2(pos.x * cosf(quad.rotation) - pos.y * sinf(quad.rotation),
            pos.x * sinf(quad.rotation) + pos.y * cosf(quad.rotation));
        pos = wdl_v2_mul(pos, quad.size);
        pos = wdl_v2_add(pos, quad.pos);

        WDL_Vec4 pos_v4 = wdl_v4(pos.x, pos.y, 0.0f, 1.0f);
        pos_v4 = wdl_m4_mul_vec(proj, pos_v4);

        br->vertices[br->curr_quad * 4 + i] = (Vertex) {
            .pos = wdl_v2(pos_v4.x, pos_v4.y),
            .uv = uv[i],
            .color = quad.color,
        };
    }

    br->curr_quad++;
}
