#ifndef ASSMAN_H
#define ASSMAN_H

#include "waddle.h"
#include "graphics.h"

typedef enum AssetType {
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_FONT,

    ASSET_TYPE_COUNT,
} AssetType;

typedef struct AssetDesc AssetDesc;
struct AssetDesc {
    WDL_Str name;
    WDL_Str filepath;
    AssetType type;

    GfxTextureSampler texture_sampler;
};

extern void assman_init(void);
extern void assman_terminate(void);

extern void asset_load(AssetDesc desc);

#define asset_get(NAME, TYPE, RETURN_TYPE) ({ \
        RETURN_TYPE result; \
        _asset_get_impl((NAME), (TYPE), &result); \
        result; \
    })

extern void _asset_get_impl(WDL_Str name, AssetType type, void* output);

#endif // ASSMAN_H
