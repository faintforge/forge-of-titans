#ifndef ASSMAN_H
#define ASSMAN_H

#include "waddle.h"

typedef enum AssetType {
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_FONT,

    ASSET_TYPE_COUNT,
} AssetType;

extern void assman_init(void);
extern void assman_terminate(void);

extern void  asset_load(WDL_Str name, WDL_Str filepath, AssetType type);
extern void* asset_get(WDL_Str name, AssetType type);

#endif // ASSMAN_H
