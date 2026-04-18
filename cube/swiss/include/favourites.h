/* favourites.h — v2 */
#ifndef FAVOURITES_H
#define FAVOURITES_H
#include <stdbool.h>
#include "devices/deviceHandler.h"

#define MAX_FAVOURITES    64
#define FAV_FILE_PATH     "swiss/settings/favourites.ini"
#define FAV_DISPLAY_NAME  48

typedef struct {
    char path[PATHNAME_MAX];
    char name[FAV_DISPLAY_NAME];
} FavouriteEntry;

void            favourites_load(void);
void            favourites_save(void);
bool            favourites_add(const char *path, const char *name);
bool            favourites_remove(const char *path);
bool            favourites_contains(const char *path);
FavouriteEntry *favourites_get_list(void);
int             favourites_get_count(void);
void            favourites_show_ui(void);

#endif
