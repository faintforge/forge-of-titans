#include "assman.h"
#include "waddle.h"
#include "font.h"

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
    void* (*load)(WDL_Str filepath);
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

static void* load_texture(WDL_Str filepath) {
    (void) filepath;
    return NULL;
}

static void* font_load(WDL_Str filepath) {
    return font_create(assman.arena, filepath);
}

void assman_init(void) {
    wdl_assert(!assman.inited, "Asset manager already initialized.");

    WDL_Arena* arena = wdl_arena_create();
    assman = (AssetManager) {
        .inited = true,
        .arena = arena,
        .loaders = {
            [ASSET_TYPE_TEXTURE] = {
                .load = load_texture,
            },
            [ASSET_TYPE_FONT] = {
                .load = font_load,
                .unload = (void (*)(void*)) font_destroy,
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
}

void asset_load(WDL_Str name, WDL_Str filepath, AssetType type) {
    wdl_assert(assman.inited, "Asset manager not initialized.");
    wdl_assert(type < ASSET_TYPE_COUNT, "Unknown asset type.");

    Asset asset = {
        .type = type,
        .data = assman.loaders[type].load(filepath),
    };
    b8 new = wdl_hm_insert(assman.asset_map, name, asset);
    if (!new) {
        wdl_warn("Asset %.*s (%s) already loaded!", name.len, name.data, asset_type_to_cstr(type));
    }
}

void* asset_get(WDL_Str name, AssetType type) {
    wdl_assert(assman.inited, "Asset manager not initialized.");
    wdl_assert(type < ASSET_TYPE_COUNT, "Unknown asset type.");

    Asset* asset = wdl_hm_getp(assman.asset_map, name);
    if (asset == NULL) {
        wdl_warn("Asset %.*s (%s) not found!", name.len, name.data, asset_type_to_cstr(type));
        return NULL;
    }

    if (asset->type != type) {
        wdl_warn("Asset %.*s (%s) requested as a %s!", name.len, name.data, asset_type_to_cstr(asset->type), asset_type_to_cstr(type));
        return NULL;
    }

    return asset->data;
}
