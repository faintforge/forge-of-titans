#include "utils.h"

#include <stdio.h>

WDL_Str read_file(WDL_Arena* arena, WDL_Str filename) {
    const char* cstr_filename = wdl_str_to_cstr(arena, filename);
    FILE *fp = fopen(cstr_filename, "rb");
    // Pop off the cstr_filename from the arena since it's no longer needed.
    wdl_arena_pop(arena, filename.len + 1);
    if (fp == NULL) {
        wdl_error("Failed to open file '%.*s'.", filename.len, filename.data);
        return (WDL_Str) {0};
    }

    fseek(fp, 0, SEEK_END);
    u32 len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    u8* content = wdl_arena_push(arena, len);
    fread(content, sizeof(u8), len, fp);

    return wdl_str(content, len);
}
