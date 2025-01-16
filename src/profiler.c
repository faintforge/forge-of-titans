#include "profiler.h"
#include "waddle.h"

typedef struct Entry Entry;
struct Entry {
    WDL_Str name;
    struct {
        f64 inclusive; // Time of children are included.
        f64 exclusive; // Time spent inside only this function.
    } times;
    u32 call_count;

    // Hierarchy
    Entry* parent;
    Entry* next;
    WDL_HashMap* child_map;

    f64 start_time;
};

typedef struct Profiler Profiler;
struct Profiler {
    WDL_Arena* arena;
    WDL_HashMap* entry_map;
    u32 current_frame;
    Entry* entry_stack;
};

static Profiler prof = {0};

void profiler_init(void) {
    WDL_Arena* arena = wdl_arena_create();
    prof = (Profiler) {
        .arena = arena,
        .entry_map = wdl_hm_new(wdl_hm_desc_str(arena, 64, Entry)),
    };
}

void profiler_terminate(void) {
    wdl_arena_destroy(prof.arena);
}

void profiler_begin_frame(u32 frame_number) {
    prof.current_frame = frame_number;
}

void profiler_end_frame(void) {
    wdl_arena_clear(prof.arena);
    prof.entry_map = wdl_hm_new(wdl_hm_desc_str(prof.arena, 64, Entry));
    prof.entry_stack = NULL;
}

static void print_entry_map(WDL_HashMap* map, u32 depth) {
    char spaces[256] = {0};
    for (u32 i = 0; i < depth * 4; i++) {
        spaces[i] = ' ';
    }

    WDL_HashMapIter iter = wdl_hm_iter_new(map);
    while (wdl_hm_iter_valid(iter)) {
        Entry* entry = wdl_hm_iter_get_valuep(iter);
        wdl_info("%s%.*s - %u calls - %.2f ms",
                spaces,
                entry->name.len,
                entry->name.data,
                entry->call_count,
                entry->times.inclusive * 1000.0f);
        print_entry_map(entry->child_map, depth + 1);

        iter = wdl_hm_iter_next(iter);
    }
}

void profiler_dump_frame(void) {
    wdl_info("-- Profiler dump of frame %u ------------------------------------", prof.current_frame);
    print_entry_map(prof.entry_map, 0);
}

void prof_begin(WDL_Str name) {
    Entry* entry;
    WDL_HashMap* map;
    if (prof.entry_stack == NULL) {
        map = prof.entry_map;
    } else {
        map = prof.entry_stack->child_map;
    }

    entry = wdl_hm_getp(map, name);
    if (entry == NULL) {
        wdl_hm_insert(map, name, (Entry) {0});
        entry = wdl_hm_getp(map, name);
        entry->child_map = wdl_hm_new(wdl_hm_desc_str(prof.arena, 8, Entry));
        entry->name = name;
    }

    entry->start_time = wdl_os_get_time();
    entry->call_count++;

    entry->next = prof.entry_stack;
    prof.entry_stack = entry;
}

void prof_end(void) {
    Entry* entry = prof.entry_stack;
    prof.entry_stack = prof.entry_stack->next;
    entry->times.inclusive += wdl_os_get_time() - entry->start_time;
}
