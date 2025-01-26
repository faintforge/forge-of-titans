#include "engine.h"
#include "engine/assman.h"
#include "engine/graphics.h"
#include "waddle.h"

typedef enum EntityType {
    ENTITY_PLAYER,
} EntityType;

typedef struct Entity Entity;
struct Entity {
    EntityType type;
    b8 alive;

    // Transform
    WDL_Vec2 pos;
    WDL_Vec2 size;
    f32 rot;
    WDL_Vec2 pivot;

    // Rendering
    b8 renderable;
    Texture* texture;
    Color color;

    // Physics
    WDL_Vec2 vel;
    b8 grounded;
};

typedef struct Game Game;
struct Game {
    f32 dt;
    Entity entities[512];
    Camera cam;
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

    game->cam = (Camera) {
        .zoom = 24.0f,
        .pos = wdl_v2(0.0f, 0.0f),
        .invert_y = false,
    };

    // Player
    game->entities[0] = (Entity) {
        .type = ENTITY_PLAYER,
        .alive = true,

        .pos = wdl_v2(0.0f, 0.0f),
        .size = wdl_v2(1.0f, 2.0f),
        .rot = 0.0f,
        .pivot = wdl_v2(0.0f, -1.0f),

        .renderable = true,
        .texture = asset_get(wdl_str_lit("player"), ASSET_TYPE_TEXTURE, Texture*),
        .color = COLOR_WHITE,
    };
}

#define sign(V) ((V) > 0 ? 1 : (V) < 0 ? -1 : 0)
#define lerp(A, B, T) ((A) + ((B) - (A)) * (T))

void app_update(EngineCtx* ctx) {
    Game* game = ctx->user_ptr;
    Renderer* renderer = ctx->renderer;
    game->cam.screen_size = ctx->window_size;

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

    // Player controller
    for (u32 i = 0; i < wdl_arrlen(game->entities); i++) {
        Entity* ent = &game->entities[i];
        if (!ent->alive || ent->type != ENTITY_PLAYER) {
            continue;
        }

        //
        // Gravity
        //
        ent->vel.y -= 9.82f * game->dt * 5.0f;

        //
        // Horizontal movement
        //
        const f32 max_speed = 25.0f;
        const f32 acc = 80.0f;
        const f32 turn_boost = 150.0f;
        const f32 dec = 100.0f;
        const f32 jump_height = 25.0f;

        // Input
        f32 hori = 0.0f;
        hori -= key_down(ctx, KEY_A);
        hori += key_down(ctx, KEY_D);

        // Accelerate
        if (hori != 0.0f) {
            // Turn
            if (sign(hori) != sign(ent->vel.x) && ent->vel.x != 0.0f) {
                ent->vel.x += turn_boost * hori * game->dt;
            }

            ent->vel.x += acc * hori * game->dt;
            ent->vel.x = wdl_clamp(ent->vel.x, -max_speed, max_speed);
        } else {
            if (fabsf(ent->vel.x) < dec * game->dt) {
                ent->vel.x = 0.0f;
            } else {
                ent->vel.x -= dec * sign(ent->vel.x) * game->dt;
            }
        }

        //
        // Vertical movement
        //
        if (key_down(ctx, KEY_SPACE) && ent->grounded) {
            ent->vel.y = jump_height;
            ent->grounded = false;
        }

        // Variable jump height
        if (!key_down(ctx, KEY_SPACE) && ent->vel.y > 0.0f) {
            ent->vel.y -= 50.0f * game->dt;
        }

        // Move
        ent->pos = wdl_v2_add(ent->pos, wdl_v2_muls(ent->vel, game->dt));

        // Ground checking
        if (ent->pos.y < 0.0f) {
            ent->pos.y = 0.0f;
            ent->vel.y = 0.0f;
            ent->grounded = true;
        }

        // Camera follow
        game->cam.pos.x = lerp(game->cam.pos.x, ent->pos.x, game->dt * 4.0f);
        game->cam.pos.y = lerp(game->cam.pos.y, ent->pos.y, game->dt * 4.0f);
    }

    if (key_down(ctx, KEY_DOWN)) {
        game->cam.zoom += 100.0f * game->dt;
    }
    if (key_down(ctx, KEY_UP)) {
        game->cam.zoom -= 100.0f * game->dt;
    }

    // Rendering
    renderer_begin(renderer, game->cam);

    f32 aspect = (f32) game->cam.screen_size.x / (f32) game->cam.screen_size.y;
    WDL_Vec2 full_screen_quad_size = wdl_v2(aspect * game->cam.zoom, game->cam.zoom + 2.0f);
    renderer_draw_quad(renderer, wdl_v2s(0.0f), game->cam.pos, full_screen_quad_size, 0.0, COLOR_BLACK);

    const WDL_Ivec2 grid = wdl_iv2(100, 4);
    for (i32 y = 0; y < grid.y; y++) {
        for (i32 x = 0; x < grid.x; x++) {
            Color color;
            if ((x + y) % 2 == 0) {
                color = color_rgb_hex(0x211818);
            } else {
                color = color_rgb_hex(0x182118);
            }
            f32 x0 = x - grid.x / 2.0f + 0.5f;
            f32 y0 = -y;
            renderer_draw_quad(renderer, wdl_v2(0.0f, 1.0f), wdl_v2(x0, y0), wdl_v2s(1.0f), 0.0f, color);
        }
    }

    for (u32 i = 0; i < wdl_arrlen(game->entities); i++) {
        Entity* ent = &game->entities[i];
        if (!ent->alive) {
            continue;
        }
        renderer_draw_quad(renderer, ent->pivot, ent->pos, ent->size, ent->rot, ent->color);
    }

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
                .size = wdl_iv2(1280, 720),
                .title = wdl_str_lit("Forge of Titans"),
                .resizable = false,
                .vsync = true,
            },
            .startup = app_startup,
            .update = app_update,
            .shutdown = app_shutdown,
        });
}
