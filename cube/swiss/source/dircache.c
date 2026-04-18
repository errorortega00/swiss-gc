/* dircache.c - LRU directory cache for Swiss GC */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include "dircache.h"

static DirCacheSlot s_cache[DIRCACHE_SLOTS];
static bool         s_initialized = false;
static u32          s_hits        = 0;
static u32          s_misses      = 0;

static void cache_init(void) {
    memset(s_cache, 0, sizeof(s_cache));
    s_initialized = true;
}

static int find_lru_slot(void) {
    u64 oldest = UINT64_MAX;
    int idx    = 0;
    for (int i = 0; i < DIRCACHE_SLOTS; i++) {
        if (!s_cache[i].valid) return i;
        if (s_cache[i].timestamp < oldest) {
            oldest = s_cache[i].timestamp;
            idx    = i;
        }
    }
    return idx;
}

static void free_slot(int i) {
    if (s_cache[i].entries) {
        free(s_cache[i].entries);
        s_cache[i].entries = NULL;
    }
    s_cache[i].valid = false;
    s_cache[i].count = 0;
}

int dircache_lookup(const char *path, DEVICEHANDLER_INTERFACE *dev,
                    file_handle **out) {
    if (!s_initialized) cache_init();
    *out = NULL;

    for (int i = 0; i < DIRCACHE_SLOTS; i++) {
        if (!s_cache[i].valid)             continue;
        if (s_cache[i].device != dev)      continue;
        if (strcmp(s_cache[i].path, path)) continue;

        int count = s_cache[i].count;
        file_handle *copy = calloc(count, sizeof(file_handle));
        if (!copy) {
            s_misses++;
            return -1;
        }
        memcpy(copy, s_cache[i].entries, count * sizeof(file_handle));
        s_cache[i].timestamp = gettime();
        *out = copy;
        s_hits++;
        return count;
    }

    s_misses++;
    return -1;
}

void dircache_store(const char *path, DEVICEHANDLER_INTERFACE *dev,
                    file_handle *entries, int count) {
    if (!s_initialized) cache_init();
    if (count <= 0 || count > DIRCACHE_MAX_FILES) return;

    int idx = find_lru_slot();
    free_slot(idx);

    s_cache[idx].entries = calloc(count, sizeof(file_handle));
    if (!s_cache[idx].entries) return;

    memcpy(s_cache[idx].entries, entries, count * sizeof(file_handle));
    strncpy(s_cache[idx].path, path, PATHNAME_MAX - 1);
    s_cache[idx].path[PATHNAME_MAX - 1] = '\0';
    s_cache[idx].count     = count;
    s_cache[idx].device    = dev;
    s_cache[idx].valid     = true;
    s_cache[idx].timestamp = gettime();
}

void dircache_invalidate(void) {
    if (!s_initialized) { cache_init(); return; }
    for (int i = 0; i < DIRCACHE_SLOTS; i++) free_slot(i);
    s_hits = s_misses = 0;
}

void dircache_invalidate_path(const char *path) {
    if (!s_initialized) return;
    for (int i = 0; i < DIRCACHE_SLOTS; i++)
        if (s_cache[i].valid && !strcmp(s_cache[i].path, path))
            free_slot(i);
}
