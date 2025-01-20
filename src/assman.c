#include "assman.h"
#include "graphics.h"
#include "waddle.h"
#include "font.h"

#include <string.h>

#include <stb_image.h>

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

typedef struct AssetLoader AssetLoader;
struct AssetLoader {
    u64 asset_size;
    void* (*load)(AssetDesc desc);
    void (*unload)(void* data);
};

typedef struct Asset Asset;
struct Asset {
    AssetType type;
    void* data;
};

typedef struct AssetManager AssetManager;
struct AssetManager {
    b8 inited;
    WDL_Arena* arena;
    AssetLoader loaders[ASSET_TYPE_COUNT];
    // Key: WDL_Str
    // Value: Asset
    WDL_HashMap* asset_map;
};

static AssetManager assman = {0};

static void* texture_load(AssetDesc desc) {
    WDL_Scratch scratch = wdl_scratch_begin(NULL, 0);
    const char* cstr = wdl_str_to_cstr(scratch.arena, desc.filepath);
    WDL_Ivec2 size;
    i32 channels;
    u8* data = stbi_load(cstr, &size.x, &size.y, &channels, 0);
    if (data == NULL) {
        wdl_error("Texture %.*s not found!", desc.filepath.len, desc.filepath.data);
        return NULL;
    }
    GfxTexture* texture = wdl_arena_push_no_zero(assman.arena, sizeof(GfxTexture));
    *texture = gfx_texture_new((GfxTextureDesc) {
            .data = data,
            .format = channels,
            .size = size,
            .sampler = desc.texture_sampler,
        });
    stbi_image_free(data);
    wdl_scratch_end(scratch);
    return texture;
}

static void texture_unload(void* data) {
    (void) data;
    wdl_warn("Maybe implement texture unloading!");
}

static void* font_load(AssetDesc desc) {
    Font** font = wdl_arena_push_no_zero(assman.arena, sizeof(Font*));
    *font = font_create(assman.arena, desc.filepath);
    return font;
}

static void font_unload(void* data) {
    Font** font = data;
    font_destroy(*font);
}

void assman_init(void) {
    wdl_assert(!assman.inited, "Asset manager already initialized.");

    WDL_Arena* arena = wdl_arena_create();
    assman = (AssetManager) {
        .inited = true,
        .arena = arena,
        .loaders = {
            [ASSET_TYPE_TEXTURE] = {
                .asset_size = sizeof(GfxTexture),
                .load = texture_load,
                .unload = texture_unload,
            },
            [ASSET_TYPE_FONT] = {
                .asset_size = sizeof(Font*),
                .load = font_load,
                .unload = font_unload,
            },
        },
        .asset_map = wdl_hm_new(wdl_hm_desc_str(arena, 512, Asset)),
    };
}

void assman_terminate(void) {
    wdl_assert(assman.inited, "Asset manager not initialized.");

    WDL_HashMapIter iter = wdl_hm_iter_new(assman.asset_map);
    while (wdl_hm_iter_valid(iter)) {
        Asset* asset = wdl_hm_iter_get_valuep(iter);
        assman.loaders[asset->type].unload(asset->data);
        iter = wdl_hm_iter_next(iter);
    }

    wdl_arena_destroy(assman.arena);
}

void asset_load(AssetDesc desc) {
    wdl_assert(assman.inited, "Asset manager not initialized.");
    wdl_assert(desc.type < ASSET_TYPE_COUNT, "Unknown asset type.");

    Asset asset = {
        .type = desc.type,
        .data = assman.loaders[desc.type].load(desc),
    };
    b8 new = wdl_hm_insert(assman.asset_map, desc.name, asset);
    if (!new) {
        wdl_warn("Asset %.*s (%s) already loaded!", desc.name.len, desc.name.data, asset_type_to_cstr(desc.type));
    }
}

void _asset_get_impl(WDL_Str name, AssetType type, void* output) {
    wdl_assert(assman.inited, "Asset manager not initialized.");
    wdl_assert(type < ASSET_TYPE_COUNT, "Unknown asset type.");

    Asset* asset = wdl_hm_getp(assman.asset_map, name);
    if (asset == NULL) {
        wdl_warn("Asset %.*s (%s) not found!", name.len, name.data, asset_type_to_cstr(type));
        return;
    }

    if (asset->type != type) {
        wdl_warn("Asset %.*s (%s) requested as a %s!", name.len, name.data, asset_type_to_cstr(asset->type), asset_type_to_cstr(type));
        return;
    }

    memcpy(output, asset->data, assman.loaders[type].asset_size);
}
