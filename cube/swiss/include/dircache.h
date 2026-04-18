/* dircache.h — v2 */
#ifndef DIRCACHE_H
#define DIRCACHE_H
#include <stdbool.h>
#include "devices/deviceHandler.h"

#define DIRCACHE_SLOTS     4
#define DIRCACHE_MAX_FILES 512

typedef struct {
    char                  path[PATHNAME_MAX];
    file_handle          *entries;
    int                   count;
    bool                  valid;
    u64                   timestamp;
    DEVICEHANDLER_INTERFACE *device;
} DirCacheSlot;

/* Retorna copia del caché (caller hace free). -1 si miss. */
int  dircache_lookup(const char *path, DEVICEHANDLER_INTERFACE *dev,
                     file_handle **out);
void dircache_store(const char *path, DEVICEHANDLER_INTERFACE *dev,
                    file_handle *entries, int count);
void dircache_invalidate(void);
void dircache_invalidate_path(const char *path);

#endif
