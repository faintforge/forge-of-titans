#include "engine.h"
#include "engine/assman.h"
#include "waddle.h"

typedef struct Entity Entity;
struct Entity {
    // Transform
    WDL_Vec2 pos;
    WDL_Vec2 size;
    f32 rot;

    // Rendering
    Texture* texture;
    Color color;
};

typedef struct Game Game;
struct Game {
    f32 dt;
    Entity entities[512];
};

void app_startup(EngineCtx* ctx) {
    WDL_Arena* persistent = ctx->persistent_arena;
    Game* game = wdl_arena_push(persistent, sizeof(Game));
    ctx->user_ptr = game;

    // Assets
    asset_load((AssetDesc) {
            .name = wdl_str_lit("player"),
            .filepath = wdl_str_lit("assets/textures/player.png"),
            .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            .type = ASSET_TYPE_TEXTURE,
        });
    asset_load((AssetDesc) {
            .name = wdl_str_lit("tiny5"),
            .filepath = wdl_str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"),
            .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            .type = ASSET_TYPE_FONT,
        });

    // Player
    game->entities[0] = (Entity) {
        .pos = wdl_v2(0.0f, 0.0f),
        .size = wdl_v2(1.0f, 2.0f),
        .rot = 0.0f,

        .texture = asset_get(wdl_str_lit("player"), ASSET_TYPE_TEXTURE, Texture*),
        .color = COLOR_WHITE,
    };
}

void app_update(EngineCtx* ctx) {
    Game* game = ctx->user_ptr;
    Renderer* renderer = ctx->renderer;

    // Delta time
    static f32 last = 0.0f;
    f32 curr = wdl_os_get_time();
    game->dt = curr - last;
    last = curr;

    // FPS
    static f32 fps_timer = 0.0f;
    static u32 fps = 0;
    fps_timer += game->dt;
    fps++;
    if (fps_timer >= 1.0f) {
        wdl_info("FPS: %u", fps);
        fps = 0;
        fps_timer = 0.0f;
    }

    // Rendering
    Camera cam = {
        .zoom = 16.0f,
        .pos = wdl_v2(0.0f, 0.0f),
        .invert_y = false,
        .screen_size = ctx->window_size,
    };

    renderer_begin(renderer, cam);

    f32 aspect = (f32) cam.screen_size.x / (f32) cam.screen_size.y;
    WDL_Vec2 full_screen_quad_size = wdl_v2(aspect * cam.zoom, cam.zoom);
    renderer_draw_quad(renderer, wdl_v2s(0.0f), full_screen_quad_size, 0.0, COLOR_BLACK);

    renderer_draw_quad(renderer, wdl_v2(0.0f, 0.0f), wdl_v2s(1.0f), 0.0f, COLOR_WHITE);

    renderer_end(renderer);
}

void app_shutdown(EngineCtx* ctx) {
    (void) ctx;
    wdl_dump_arena_metrics();
}

// NOTE: GLFW calls any function called shutdown.
// void shutdown(EngineCtx* ctx) {
//     (void) ctx;
//     wdl_debug("Called from GLFW!");
// }

i32 main(void) {
    return engine_run((ApplicationDesc) {
            .window = {
                .size = wdl_iv2(800, 600),
                .title = wdl_str_lit("Forge of Titans"),
                .resizable = false,
                .vsync = true,
            },
            .startup = app_startup,
            .update = app_update,
            .shutdown = app_shutdown,
        });
}
