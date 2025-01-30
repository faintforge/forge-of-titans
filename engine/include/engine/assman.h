#ifndef ASSMAN_H
#define ASSMAN_H

#include "waddle.h"
#include "graphics.h"
#include "font.h"

extern void assman_init(void);
extern void assman_terminate(void);

extern GfxTexture asset_load_texture(WDL_Str name, WDL_Str filepath, GfxTextureSampler sampler);
extern Font*      asset_load_font(WDL_Str name, WDL_Str filepath);

extern GfxTexture asset_get_texture(WDL_Str name);
extern Font*      asset_get_font(WDL_Str name);

#endif // ASSMAN_H
