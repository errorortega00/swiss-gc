/* files.c — v2 (corregido)
 *
 * BUGS CORREGIDOS vs v1:
 *   - El flag curFromCache fue eliminado. dircache_lookup() ahora
 *     devuelve una COPIA del buffer (no puntero compartido),
 *     por lo que freeFiles() puede liberar normalmente siempre.
 *   - La lógica de freeFiles() es idéntica al original — más segura.
 *   - dircache_store() se llama con los datos leídos del dispositivo.
 */

#include <fnmatch.h>
#include <gccore.h>
#include <stdarg.h>
#include <stdlib.h>
#include "swiss.h"
#include "main.h"
#include "dvd.h"
#include "files.h"
#include "dircache.h"
#include "devices/filemeta.h"

static file_handle **sortedDirEntries;
static file_handle  *curDirEntries;
static int           sortedDirEntryCount;
static int           curDirEntryCount;

int fileComparator(const void *a1, const void *b1)
{
    const file_handle *a = *(const file_handle **)a1;
    const file_handle *b = *(const file_handle **)b1;

    if ((devices[DEVICE_CUR] == &__device_dvd) &&
        ((dvdDiscTypeInt == ISO9660_GAMECUBE_DISC) ||
         (dvdDiscTypeInt == GAMECUBE_DISC) ||
         (dvdDiscTypeInt == MULTIDISC_DISC)))
    {
        if (a->size == DISC_SIZE && a->fileBase == 0) return -1;
        if (b->size == DISC_SIZE && b->fileBase == 0) return  1;
    }

    if (a->fileType != b->fileType) return b->fileType - a->fileType;
    return strcasecmp(a->name, b->name);
}

int sortFiles(file_handle *dir, int numFiles, file_handle ***sortedDir)
{
    int i = 0;
    *sortedDir = calloc(numFiles, sizeof(file_handle *));
    if (*sortedDir) {
        for (int j = 0; j < numFiles; j++) {
            switch (dir[j].fileType) {
                case IS_FILE:
                    if (!checkExtension(dir[j].name,
                                        devices[DEVICE_CUR]->extraExtensions))
                        continue;
                case IS_DIR:
                    if (!swissSettings.showHiddenFiles &&
                        ((dir[j].fileAttrib & ATTRIB_HIDDEN) ||
                         *getRelativeName(dir[j].name) == '.'))
                        continue;
                default:
                    break;
            }
            (*sortedDir)[i++] = &dir[j];
        }
        qsort(*sortedDir, i, sizeof(file_handle *), fileComparator);
    }
    return i;
}

/*
 * freeFiles — igual al original: siempre libera curDirEntries.
 * Es seguro porque dircache_lookup() devuelve una COPIA,
 * no un puntero al buffer interno del caché.
 */
void freeFiles() {
    free(sortedDirEntries);
    sortedDirEntries = NULL;
    sortedDirEntryCount = 0;
    if (curDirEntries) {
        for (int i = 0; i < curDirEntryCount; i++) {
            if (curDirEntries[i].meta) {
                meta_free(curDirEntries[i].meta);
                curDirEntries[i].meta = NULL;
            }
            devices[DEVICE_CUR]->closeFile(&curDirEntries[i]);
        }
        free(curDirEntries);
        curDirEntries = NULL;
        curDirEntryCount = 0;
    }
}

void scanFiles() {
    freeFiles();
    print_debug("Reading directory: %s\n", curDir.name);

    /* ── Consultar caché antes de leer la SD ─────────────────────── */
    file_handle *cachedEntries = NULL;
    int cachedCount = dircache_lookup(curDir.name, devices[DEVICE_CUR],
                                      &cachedEntries);

    if (cachedCount >= 0 && cachedEntries != NULL) {
        /*
         * Cache hit: dircache_lookup hizo copia defensiva.
         * curDirEntries es dueño de este buffer y freeFiles() lo libera.
         */
        curDirEntries    = cachedEntries;
        curDirEntryCount = cachedCount;
        print_debug("dircache: %d entries from cache\n", cachedCount);
    }
    /* ── Miss: leer del dispositivo ─────────────────────────────── */
    else {
        curDirEntryCount = devices[DEVICE_CUR]->readDir(
            &curDir, &curDirEntries, -1);

        /* flattenDir: expandir subdirectorios (lógica original) */
        if (!fnmatch(swissSettings.flattenDir, curDir.name,
                     FNM_PATHNAME | FNM_CASEFOLD)) {
            for (int i = 0; i < curDirEntryCount; i++) {
                if (curDirEntries[i].fileType == IS_DIR) {
                    file_handle *dirEntries  = NULL;
                    int dirEntryCount = devices[DEVICE_CUR]->readDir(
                        &curDirEntries[i], &dirEntries, -1);
                    if (dirEntryCount > 1) {
                        curDirEntryCount--; dirEntryCount--;
                        curDirEntries = reallocarray(
                            curDirEntries,
                            curDirEntryCount + dirEntryCount,
                            sizeof(file_handle));
                        memmove(&curDirEntries[i + dirEntryCount],
                                &curDirEntries[i + 1],
                                (curDirEntryCount - i) * sizeof(file_handle));
                        memmove(&curDirEntries[i], &dirEntries[1],
                                dirEntryCount * sizeof(file_handle));
                        curDirEntryCount += dirEntryCount;
                        i--;
                    }
                    free(dirEntries);
                }
            }
        }

        /* Guardar en caché para próximas visitas */
        if (curDirEntryCount > 0) {
            dircache_store(curDir.name, devices[DEVICE_CUR],
                           curDirEntries, curDirEntryCount);
        }
    }

    if (curDirEntryCount > 0) {
        char *pwd = getenv("PWD");
        if (pwd) setenv("OLDPWD", pwd, 1);
        pwd = getExternalPath(curDir.name);
        if (pwd) setenv("PWD", pwd, 1);
        free(pwd);
    }

    print_debug("Found %i entries\n", curDirEntryCount);
    sortedDirEntryCount = sortFiles(curDirEntries, curDirEntryCount,
                                    &sortedDirEntries);
    for (int i = 0; i < sortedDirEntryCount; i++) {
        if (!strcmp(sortedDirEntries[i]->name, curFile.name)) {
            curSelection = i;
            break;
        }
    }
    memcpy(&curFile, &curDir, sizeof(file_handle));
}

/* Funciones de acceso — sin cambios vs original */
file_handle **getSortedDirEntries()  { return sortedDirEntries;    }
file_handle  *getCurrentDirEntries() { return curDirEntries;       }
int getSortedDirEntryCount()         { return sortedDirEntryCount; }
int getCurrentDirEntryCount()        { return curDirEntryCount;    }

int getSortedDirEntryIndex(file_handle *file) {
    for (int i = 0; i < sortedDirEntryCount; i++)
        if (file == sortedDirEntries[i]) return i;
    return 0;
}

u64 getCurrentDirSize() {
    u64 size = 0;
    for (int i = 0; i < curDirEntryCount; i++)
        size += curDirEntries[i].size;
    return size;
}

size_t concat_path(char *pathName, const char *dirName, const char *baseName)
{
    size_t len;
    if (pathName == dirName) len = strlen(pathName);
    else                     len = strlcpy(pathName, dirName, PATHNAME_MAX);
    if (len >= PATHNAME_MAX) return len;
    if (len) {
        if (pathName[len-1] != '/' && baseName[0] != '/') {
            if (len + 1 >= PATHNAME_MAX) return len + 1 + strlen(baseName);
            pathName[len++] = '/';
            pathName[len]   = '\0';
        } else if (pathName[len-1] == '/' && baseName[0] == '/')
            baseName++;
    }
    return strlcat(pathName, baseName, PATHNAME_MAX);
}

size_t concatf_path(char *pathName, const char *dirName,
                    const char *baseName, ...)
{
    size_t len;
    if (pathName == dirName) len = strlen(pathName);
    else                     len = strlcpy(pathName, dirName, PATHNAME_MAX);
    if (len >= PATHNAME_MAX) return len;
    if (len) {
        if (pathName[len-1] != '/' && baseName[0] != '/') {
            if (len + 1 >= PATHNAME_MAX) {
                va_list args; va_start(args, baseName);
                len += vsnprintf(NULL, 0, baseName, args);
                va_end(args);
                return len + 1;
            }
            pathName[len++] = '/';
            pathName[len]   = '\0';
        } else if (pathName[len-1] == '/' && baseName[0] == '/')
            baseName++;
    }
    va_list args; va_start(args, baseName);
    len += vsnprintf(pathName + len, PATHNAME_MAX - len, baseName, args);
    va_end(args);
    return len;
}

void ensure_path(int deviceSlot, char *path, char *oldPath, bool hidden) {
    file_handle fhFullPath = { .fileType = IS_DIR };
    concat_path(fhFullPath.name, devices[deviceSlot]->initial->name, path);
    if (oldPath) {
        file_handle fhOldFullPath = { .fileType = IS_DIR };
        concat_path(fhOldFullPath.name, devices[deviceSlot]->initial->name, oldPath);
        if (devices[deviceSlot]->renameFile) {
            if (devices[deviceSlot]->renameFile(&fhOldFullPath, fhFullPath.name))
                if (devices[deviceSlot]->makeDir)
                    devices[deviceSlot]->makeDir(&fhFullPath);
        } else if (devices[deviceSlot]->makeDir)
            devices[deviceSlot]->makeDir(&fhFullPath);
    } else if (devices[deviceSlot]->makeDir)
        devices[deviceSlot]->makeDir(&fhFullPath);
    if (devices[deviceSlot]->hideFile)
        devices[deviceSlot]->hideFile(&fhFullPath, hidden);
}
