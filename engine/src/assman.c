#include "engine/assman.h"
#include "engine/graphics.h"
#include "engine/font.h"
#include "waddle.h"

#include <string.h>

#include <stb_image.h>

typedef enum AssetType {
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_FONT,
} AssetType;

static const char* asset_type_to_cstr(AssetType type) {
    switch (type) {
        case ASSET_TYPE_TEXTURE:
            return "texture";
        case ASSET_TYPE_FONT:
            return "font";
        default:
            return "Unknown";
    }
}

typedef struct Asset Asset;
struct Asset {
    AssetType type;
    union {
        GfxTexture texture;
        Font* font;
    };
};

typedef struct AssetManager AssetManager;
struct AssetManager {
    b8 inited;
    WDL_Arena* arena;
    // Key: WDL_Str
    // Value: Asset
    WDL_HashMap* asset_map;
};

static AssetManager assman = {0};

void assman_init(void) {
    wdl_assert(!assman.inited, "Asset manager already initialized.");

    WDL_Arena* arena = wdl_arena_create();
    wdl_arena_tag(arena, wdl_str_lit("assets"));
    assman = (AssetManager) {
        .inited = true,
        .arena = arena,
        .asset_map = wdl_hm_new(wdl_hm_desc_str(arena, 512, Asset)),
    };
}

void assman_terminate(void) {
    wdl_assert(assman.inited, "Asset manager not initialized.");

    WDL_HashMapIter iter = wdl_hm_iter_new(assman.asset_map);
    while (wdl_hm_iter_valid(iter)) {
        Asset* asset = wdl_hm_iter_get_valuep(iter);
        // assman.loaders[asset->type].unload(asset->data);
        switch (asset->type) {
            case ASSET_TYPE_TEXTURE:
                wdl_debug("Maybe implemented texture unloading.");
                break;
            case ASSET_TYPE_FONT:
                font_destroy(asset->font);
                break;
        }
        iter = wdl_hm_iter_next(iter);
    }

    wdl_arena_destroy(assman.arena);
}

GfxTexture asset_load_texture(WDL_Str name, WDL_Str filepath, GfxTextureSampler sampler) {
    wdl_assert(assman.inited, "Asset manager not initialized.");

    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
    const char* cstr = wdl_str_to_cstr(scratch.arena, filepath);
    WDL_Ivec2 size;
    i32 channels;
    u8* data = stbi_load(cstr, &size.x, &size.y, &channels, 0);
    if (data == NULL) {
        wdl_error("Texture %.*s not found!", filepath.len, filepath.data);
        return GFX_TEXTURE_NULL;
    }
    GfxTexture texture = gfx_texture_new((GfxTextureDesc) {
            .data = data,
            .format = channels,
            .size = size,
            .sampler = sampler,
        });
    stbi_image_free(data);
    wdl_scratch_end(scratch);

    Asset asset = {
        .type = ASSET_TYPE_TEXTURE,
        .texture = texture,
    };
    b8 new = wdl_hm_insert(assman.asset_map, name, asset);
    if (!new) {
        wdl_warn("Asset %.*s already loaded!", name.len, name.data);
        return GFX_TEXTURE_NULL;
    }

    return texture;
}

Font* asset_load_font(WDL_Str name, WDL_Str filepath) {
    wdl_assert(assman.inited, "Asset manager not initialized.");
    Font* font = font_create(assman.arena, filepath);

    Asset asset = {
        .type = ASSET_TYPE_FONT,
        .font = font,
    };
    b8 new = wdl_hm_insert(assman.asset_map, name, asset);
    if (!new) {
        wdl_warn("Asset %.*s already loaded!", name.len, name.data);
        return NULL;
    }

    return font;
}

GfxTexture asset_get_texture(WDL_Str name) {
    wdl_assert(assman.inited, "Asset manager not initialized.");
    Asset* asset = wdl_hm_getp(assman.asset_map, name);
    if (asset == NULL) {
        wdl_error("Asset %.*s not found!", name.len, name.data);
        return GFX_TEXTURE_NULL;
    }

    if (asset->type != ASSET_TYPE_TEXTURE) {
        wdl_error("Asset %.*s is a %s not a texture.", name.len, name.data, asset_type_to_cstr(asset->type));
        return GFX_TEXTURE_NULL;
    }

    return asset->texture;
}

Font* asset_get_font(WDL_Str name) {
    wdl_assert(assman.inited, "Asset manager not initialized.");
    Asset* asset = wdl_hm_getp(assman.asset_map, name);
    if (asset == NULL) {
        wdl_error("Asset %.*s not found!", name.len, name.data);
        return NULL;
    }

    if (asset->type != ASSET_TYPE_FONT) {
        wdl_error("Asset %.*s is a %s not a font.", name.len, name.data, asset_type_to_cstr(asset->type));
        return NULL;
    }

    return asset->font;
}
