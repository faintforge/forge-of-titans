#include "engine.h"
#include "engine/assman.h"
#include "engine/font.h"
#include "engine/graphics.h"
#include "waddle.h"

void startup(void) {
    asset_load_font(wdl_str_lit("roboto"), wdl_str_lit("assets/fonts/Roboto/Roboto-Regular.ttf"));
}

void update(void) {
    Renderer* rend = get_renderer();
    WDL_Vec2 half_screen = wdl_iv2_to_v2(wdl_iv2_divs(get_screen_size(), 2));
    Camera cam = {
        .screen_size = get_screen_size(),
        .pos = half_screen,
        .zoom = get_screen_size().y,
        .invert_y = true,
    };
    renderer_begin(rend, cam);

    gfx_clear(COLOR_BLACK);

    Font* font = asset_get_font(wdl_str_lit("roboto"));
    font_set_size(font, 32);
    renderer_draw_text(rend, wdl_str_lit("The quick brown fox jumps over the lazy dog."), font, wdl_v2(0.0f, 0.0f), half_screen, COLOR_WHITE);

    renderer_end(rend);
}

void shutdown(void) {}

i32 main(void) {
    return engine_run((ApplicationDesc) {
            .window = {
                .size = wdl_iv2(800, 600),
                .title = wdl_str_lit("Test Tool"),
                .resizable = false,
                .vsync = true,
            },
            .startup = startup,
            .update = update,
            .shutdown = shutdown,
        });
}
