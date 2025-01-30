#ifndef TILE_H
#define TILE_H

#include "engine.h"

typedef enum TilePos {
    TL = 1 << 0,
    T  = 1 << 1,
    TR = 1 << 2,
    L  = 1 << 3,
    R  = 1 << 4,
    BL = 1 << 5,
    B  = 1 << 6,
    BR = 1 << 7,

    COUNT = 1 << 8,
} TileNeighbor;

static const TileNeighbor TILE_NEIGHBOR_GRID[9] = {
    BL, B, BR,
    L,  0, R,
    TL, T, TR,
};

static const Sprite TILE_NEIGHBOR_SPRITE_LOOKUP[COUNT] = {
    // 0 0 0
    // 0   0
    // 0 0 0
    [0] = {
        .pos = {24, 24},
        .size = {8, 8},
    },

    //
    // Big Block
    //

    // 0 0 0
    // 0   1
    // 0 1 1
    [R | B | BR] = {
        .pos = {0, 0},
        .size = {8, 8},
    },

    // 0 0 0
    // 1   1
    // 1 1 1
    [L | R | BL | B | BR] = {
        .pos = {8, 0},
        .size = {8, 8},
    },

    // 0 0 0
    // 1   0
    // 1 1 0
    [L | BL | B] = {
        .pos = {16, 0},
        .size = {8, 8},
    },

    // 0 1 1
    // 0   1
    // 0 1 1
    [T | TR | R | B | BR] = {
        .pos = {0, 8},
        .size = {8, 8},
    },

    // 1 1 1
    // 1   1
    // 1 1 1
    [TL | T | TR | L | R | BL | B | BR] = {
        .pos = {8, 8},
        .size = {8, 8},
    },

    // 1 1 0
    // 1   0
    // 1 1 0
    [TL | T | L | BL | B] = {
        .pos = {16, 8},
        .size = {8, 8},
    },

    // 0 1 1
    // 0   1
    // 0 0 0
    [T | TR | R] = {
        .pos = {0, 16},
        .size = {8, 8},
    },

    // 1 1 1
    // 1   1
    // 0 0 0
    [TL | T | TR | L | R] = {
        .pos = {8, 16},
        .size = {8, 8},
    },

    // 1 1 0
    // 1   0
    // 0 0 0
    [TL | T | L] = {
        .pos = {16, 16},
        .size = {8, 8},
    },

    //
    // Vertical Bar
    //

    // 0 0 0
    // 0   0
    // 0 1 0
    [B] = {
        .pos = {24, 0},
        .size = {8, 8},
    },
    // 0 0 0
    // 0   0
    // 1 1 1
    [BL | B | BR] = {
        .pos = {24, 0},
        .size = {8, 8},
    },

    // 0 1 0
    // 0   0
    // 0 1 0
    [T | B] = {
        .pos = {24, 8},
        .size = {8, 8},
    },
    // 1 1 1
    // 0   0
    // 1 1 1
    [TL | T | TR | BL | B | BR] = {
        .pos = {24, 8},
        .size = {8, 8},
    },
    // 1 1 1
    // 0   0
    // 0 1 0
    [TL | T | TR | B] = {
        .pos = {24, 8},
        .size = {8, 8},
    },
    // 0 1 0
    // 0   0
    // 1 1 1
    [T | BL | B | BR] = {
        .pos = {24, 8},
        .size = {8, 8},
    },

    // 0 1 0
    // 0   0
    // 0 0 0
    [T] = {
        .pos = {24, 16},
        .size = {8, 8},
    },
    // 1 1 1
    // 0   0
    // 0 0 0
    [TR | T | TL] = {
        .pos = {24, 16},
        .size = {8, 8},
    },

    //
    // Horizontal Bar
    //

    // 0 0 0
    // 0   1
    // 0 0 0
    [R] = {
        .pos = {0, 24},
        .size = {8, 8},
    },

    // 0 0 0
    // 1   1
    // 0 0 0
    [L | R] = {
        .pos = {8, 24},
        .size = {8, 8},
    },

    // 0 0 0
    // 1   0
    // 0 0 0
    [L] = {
        .pos = {16, 24},
        .size = {8, 8},
    },

    //
    // Edge connectors
    //

    // 0 0 0
    // 0   1
    // 0 1 0
    [R | B] = {
        .pos = {32, 0},
        .size = {8, 8},
    },

    // 0 0 0
    // 1   0
    // 0 1 0
    [L | B] = {
        .pos = {40, 0},
        .size = {8, 8},
    },

    // 0 1 0
    // 0   1
    // 0 0 0
    [T | R] = {
        .pos = {32, 8},
        .size = {8, 8},
    },

    // 0 1 0
    // 1   0
    // 0 0 0
    [T | L] = {
        .pos = {40, 8},
        .size = {8, 8},
    },
};

typedef enum TileType {
    TILE_NONE,
    TILE_404,
} TileType;

#endif // TILE_H
