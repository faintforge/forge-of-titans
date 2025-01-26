#ifndef ENGINE_H
#define ENGINE_H

#include "waddle.h"

typedef struct EngineCtx EngineCtx;

typedef struct ApplicationDesc ApplicationDesc;
struct ApplicationDesc {
    struct {
        WDL_Ivec2 size;
        WDL_Str title;
        b8 resizable;
        b8 vsync;
    } window;

    void (*startup)(EngineCtx* ctx);
    void (*update)(EngineCtx* ctx);
    void (*shutdown)(EngineCtx* ctx);
};

extern i32 engine_run(ApplicationDesc app_desc);

extern WDL_Arena* get_persistent_arena(const EngineCtx* ctx);
extern WDL_Arena* get_frame_arena(const EngineCtx* ctx);

extern void  set_user_ptr(EngineCtx* ctx, void* user_ptr);
extern void* get_user_ptr(const EngineCtx* ctx);

#endif // ENGINE_H
