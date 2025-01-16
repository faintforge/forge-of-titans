#include "profiler.h"
#include "utils.h"

typedef struct Entry Entry;

typedef struct EntryMap EntryMap;
struct EntryMap {
    Entry* entires;
    b8* used;
    u32 capacity;
};

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
    Entry* hash_next; // Linked list for colliding elements in the hash map.
    EntryMap child_map;

    f64 start_time;
};

static EntryMap entry_map_new(WDL_Arena* arena, u32 capacity) {
    EntryMap map = {
        .entires = wdl_arena_push_no_zero(arena, capacity * sizeof(Entry)),
        .used = wdl_arena_push(arena, capacity * sizeof(b8)),
        .capacity = capacity,
    };
    return map;
}

typedef struct Profiler Profiler;
struct Profiler {
    WDL_Arena* arena;
    EntryMap entry_map;
    u32 current_frame;
    Entry* entry_stack;
};

static Profiler prof = {0};

static Entry* get_entry(WDL_Str name) {
    EntryMap* map = &prof.entry_map;
    if (prof.entry_stack != NULL) {
        map = &prof.entry_stack->child_map;
    }

    u64 hash = fvn1a_hash(name.data, name.len);
    u32 index = hash % map->capacity;
    Entry* entry = &map->entires[index];;
    b8 new_entry = false;
    if (!map->used[index]) {
        new_entry = true;
        map->used[index] = true;
    } else {
        b8 entry_found = wdl_str_equal(entry->name, name);
        while (!entry_found && entry->hash_next != NULL) {
            entry = entry->hash_next;
            if (wdl_str_equal(entry->name, name)) {
                entry_found = true;
                break;
            }
        }

        if (!entry_found) {
            new_entry = true;
            Entry* new_entry = wdl_arena_push_no_zero(prof.arena, sizeof(Entry));
            entry->hash_next = new_entry;
            entry = new_entry;
        }
    }

    if (new_entry) {
        *entry = (Entry) {
            // TODO: Push name onto the arena
            .name = name,
            .child_map = entry_map_new(prof.arena, 8),
            .parent = prof.entry_stack,
        };
    }

    entry->call_count++;

    return entry;
}

void profiler_init(void) {
    WDL_Arena* arena = wdl_arena_create();
    prof = (Profiler) {
        .arena = arena,
        .entry_map = entry_map_new(arena, 64),
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
    prof.entry_map = entry_map_new(prof.arena, 64);
    prof.entry_stack = NULL;
}

static void print_entry_map(EntryMap map, u32 depth) {
    char spaces[256] = {0};
    for (u32 i = 0; i < depth * 4; i++) {
        spaces[i] = ' ';
    }

    for (u32 i = 0; i < map.capacity; i++) {
        if (map.used[i]) {
            Entry* entry = &map.entires[i];
            while (entry != NULL) {
                wdl_info("%s%.*s - %u calls - %.2f ms",
                        spaces,
                        entry->name.len,
                        entry->name.data,
                        entry->call_count,
                        entry->times.inclusive * 1000.0f);
                print_entry_map(entry->child_map, depth + 1);
                entry = entry->hash_next;
            }
        }
    }
}

void profiler_dump_frame(void) {
    wdl_info("-- Profiler dump of frame %u ------------------------------------", prof.current_frame);
    print_entry_map(prof.entry_map, 0);
}

void prof_begin(WDL_Str name) {
    Entry* entry = get_entry(name);
    entry->start_time = wdl_os_get_time();

    entry->next = prof.entry_stack;
    prof.entry_stack = entry;
}

void prof_end(void) {
    Entry* entry = prof.entry_stack;
    prof.entry_stack = prof.entry_stack->next;
    entry->times.inclusive += wdl_os_get_time() - entry->start_time;
}
