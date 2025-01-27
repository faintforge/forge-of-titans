#include "engine.h"
#include "engine/assman.h"
#include "engine/graphics.h"
#include "waddle.h"
#include <threads.h>

typedef enum EntityType {
    ENTITY_NULL,
    ENTITY_PLAYER,
} EntityType;

typedef struct Entity Entity;
struct Entity {
    // Transform
    WDL_Vec2 pos;
    WDL_Vec2 size;
    f32 rot;
    WDL_Vec2 pivot;

    // Rendering
    b8 renderable;
    GfxTexture texture;
    Color color;

    // Physics
    WDL_Vec2 vel;
    b8 grounded;

    // Management
    EntityType type;
    Entity* next;
    Entity* prev;
};

const Entity ENTITY_DEFAULT = {
    .pos = {0.0f, 0.0f},
    .size = {1.0f, 1.0f},
    .rot = 0.0f,
    .pivot = {0.0f, 0.0f},

    .renderable = false,
    .texture = GFX_TEXTURE_NULL,
    .color = COLOR_WHITE,

    .vel = {0.0f, 0.0f},
    .grounded = false,

    .type = ENTITY_NULL,
    .next = NULL,
    .prev = NULL,
};

typedef struct EntityWorld EntityWorld;
struct EntityWorld {
    struct {
        Entity* first;
        Entity* last;
    } alive;
    Entity* free_list;
    WDL_Arena* arena;
};

EntityWorld entity_world_create(void) {
    EntityWorld world = {
        .arena = wdl_arena_create(),
    };
    wdl_arena_tag(world.arena, wdl_str_lit("entity"));
    return world;
}

void entity_world_destroy(EntityWorld* world) {
    wdl_arena_destroy(world->arena);
}

typedef struct Game Game;
struct Game {
    f32 dt;
    Camera cam;
    EntityWorld ent_world;
};

static Game game;

Entity* entity_spawn(void) {
    EntityWorld* world = &game.ent_world;
    Entity* ent;
    if (world->free_list != NULL) {
        ent = world->free_list;
        wdl_sll_stack_pop(world->free_list);
    } else {
        ent = wdl_arena_push_no_zero(world->arena, sizeof(Entity));
    }
    *ent = ENTITY_DEFAULT;
    wdl_dll_push_back(world->alive.first, world->alive.last, ent);
    return ent;
}

void entity_kill(Entity* ent) {
    EntityWorld* world = &game.ent_world;
    wdl_dll_remove(world->alive.first, world->alive.last, ent);
    wdl_sll_stack_push(world->free_list, ent);
}

// void func(Entity* ent)
#define iter_alive_entities for (Entity* ent = game.ent_world.alive.first; ent != NULL; ent = ent->next)

void app_startup(void) {
    game.ent_world = entity_world_create();
    game.cam = (Camera) {
        .zoom = 32.0f,
        .pos = wdl_v2(0.0f, 0.0f),
        .invert_y = false,
    };

    //
    // Assets
    //

    // Textures
    asset_load((AssetDesc) {
            .name = wdl_str_lit("player"),
            .filepath = wdl_str_lit("assets/textures/player.png"),
            .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            .type = ASSET_TYPE_TEXTURE,
        });

    asset_load((AssetDesc) {
            .name = wdl_str_lit("sky"),
            .filepath = wdl_str_lit("assets/textures/sky_bg.png"),
            .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            .type = ASSET_TYPE_TEXTURE,
        });

    // Fonts
    asset_load((AssetDesc) {
            .name = wdl_str_lit("tiny5"),
            .filepath = wdl_str_lit("assets/fonts/Tiny5/Tiny5-Regular.ttf"),
            .texture_sampler = GFX_TEXTURE_SAMPLER_NEAREST,
            .type = ASSET_TYPE_FONT,
        });

    // Player
    Entity* player = entity_spawn();
    *player = (Entity) {
        .type = ENTITY_PLAYER,

        .pos = wdl_v2(0.0f, 0.0f),
        .size = wdl_v2(1.0f, 2.0f),
        .rot = 0.0f,
        .pivot = wdl_v2(0.0f, -1.0f),

        .renderable = true,
        .texture = asset_get(wdl_str_lit("player"), ASSET_TYPE_TEXTURE, GfxTexture),
        .color = COLOR_WHITE,
    };

    Entity* enemy = entity_spawn();
    enemy->renderable = true;
}

#define sign(V) ((V) > 0 ? 1 : (V) < 0 ? -1 : 0)
#define lerp(A, B, T) ((A) + ((B) - (A)) * (T))

void app_update(void) {
    Renderer* renderer = get_renderer();
    game.cam.screen_size = get_screen_size();

    // Delta time
    static f32 last = 0.0f;
    f32 curr = wdl_os_get_time();
    game.dt = curr - last;
    last = curr;

    // FPS
    static f32 fps_timer = 0.0f;
    static u32 fps = 0;
    fps_timer += game.dt;
    fps++;
    if (fps_timer >= 1.0f) {
        wdl_info("FPS: %u", fps);
        fps = 0;
        fps_timer = 0.0f;
    }

    // Player controller
    iter_alive_entities {
        if (ent->type != ENTITY_PLAYER) {
            continue;
        }

        //
        // Gravity
        //
        ent->vel.y -= 9.82f * game.dt * 10.0f;

        //
        // Horizontal movement
        //
        const f32 max_speed = 25.0f;
        const f32 acc = 80.0f;
        const f32 turn_boost = 150.0f;
        const f32 dec = 100.0f;
        const f32 jump_height = 30.0f;

        // Input
        f32 hori = 0.0f;
        hori -= key_down(KEY_A);
        hori += key_down(KEY_D);

        // Accelerate
        if (hori != 0.0f) {
            // Turn
            if (sign(hori) != sign(ent->vel.x) && ent->vel.x != 0.0f) {
                ent->vel.x += turn_boost * hori * game.dt;
            }

            ent->vel.x += acc * hori * game.dt;
            ent->vel.x = wdl_clamp(ent->vel.x, -max_speed, max_speed);
        } else {
            if (fabsf(ent->vel.x) < dec * game.dt) {
                ent->vel.x = 0.0f;
            } else {
                ent->vel.x -= dec * sign(ent->vel.x) * game.dt;
            }
        }

        //
        // Vertical movement
        //
        if (key_down(KEY_SPACE) && ent->grounded) {
            ent->vel.y = jump_height;
            ent->grounded = false;
        }

        // Variable jump height
        if (!key_down(KEY_SPACE) && ent->vel.y > 0.0f) {
            ent->vel.y -= 50.0f * game.dt;
        }

        // Move
        ent->pos = wdl_v2_add(ent->pos, wdl_v2_muls(ent->vel, game.dt));

        i8 facing = sign(ent->vel.x);
        if (facing != 0) {
            ent->size.x = facing;
        }

        // Ground checking
        if (ent->pos.y < 0.0f) {
            ent->pos.y = 0.0f;
            ent->vel.y = 0.0f;
            ent->grounded = true;
        }

        // Camera follow
        game.cam.pos.x = lerp(game.cam.pos.x, ent->pos.x, game.dt * 4.0f);
        game.cam.pos.y = lerp(game.cam.pos.y, ent->pos.y, game.dt * 4.0f);
    }


    if (key_down(KEY_DOWN)) {
        game.cam.zoom += 100.0f * game.dt;
    }
    if (key_down(KEY_UP)) {
        game.cam.zoom -= 100.0f * game.dt;
    }

    // Rendering
    renderer_begin(renderer, game.cam);

    f32 aspect = (f32) game.cam.screen_size.x / (f32) game.cam.screen_size.y;
    WDL_Vec2 full_screen_quad_size = wdl_v2(aspect * game.cam.zoom, game.cam.zoom + 2.0f);
    renderer_draw_quad_textured(renderer, wdl_v2s(0.0f), game.cam.pos, full_screen_quad_size, 0.0, COLOR_WHITE, asset_get(wdl_str_lit("sky"), ASSET_TYPE_TEXTURE, GfxTexture));

    const WDL_Ivec2 grid = wdl_iv2(100, 4);
    for (i32 y = 0; y < grid.y; y++) {
        for (i32 x = 0; x < grid.x; x++) {
            Color color;
            if ((x + y) % 2 == 0) {
                color = COLOR_WHITE;
            } else {
                color = COLOR_BLACK;
            }
            f32 x0 = x - grid.x / 2.0f + 0.5f;
            f32 y0 = -y;
            renderer_draw_quad(renderer, wdl_v2(0.0f, 1.0f), wdl_v2(x0, y0), wdl_v2s(1.0f), 0.0f, color);
        }
    }

    iter_alive_entities {
        if (!ent->renderable) {
            continue;
        }
        renderer_draw_quad_textured(renderer, ent->pivot, ent->pos, ent->size, ent->rot, ent->color, ent->texture);
    };

    renderer_end(renderer);
}

void app_shutdown(void) {
    wdl_dump_arena_metrics();
    entity_world_destroy(&game.ent_world);
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
