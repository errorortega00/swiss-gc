/* favourites.c — v2 (corregido)
 *
 * BUGS CORREGIDOS vs v1:
 *   - Sin DrawSelectableButtonSetState (no existe en Swiss)
 *   - Patrón UI correcto: redibuja panel completo cada frame
 *     (igual que select_recent_entry en swiss.c)
 *   - padsButtonsDown() eliminado → solo padsButtonsHeld() + debounce
 *   - config_set_device() declarado extern explícitamente
 *   - Debounce correcto en cada rama de botones
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gccore.h>
#include "favourites.h"
#include "swiss.h"
#include "main.h"
#include "files.h"
#include "gui/FrameBufferMagic.h"
#include "gui/IPLFontWrite.h"
#include "devices/deviceHandler.h"
#include "config/config.h"
#include "util.h"

/* config_set_device() vive en config.c pero no está en config.h */
extern bool config_set_device(void);

/* ------------------------------------------------------------------ */
/*  Estado interno                                                      */
/* ------------------------------------------------------------------ */

static FavouriteEntry s_favList[MAX_FAVOURITES];
static int            s_favCount = 0;
static bool           s_loaded   = false;

/* ------------------------------------------------------------------ */
/*  Helper: construir ruta al archivo de favoritos                      */
/* ------------------------------------------------------------------ */

static bool get_fav_path(char *out, size_t outSize) {
    if (!config_set_device()) return false;
    if (!devices[DEVICE_CONFIG]) return false;
    snprintf(out, outSize, "%s/%s",
             devices[DEVICE_CONFIG]->initial->name,
             FAV_FILE_PATH);
    return true;
}

/* ------------------------------------------------------------------ */
/*  Carga / Guardado                                                    */
/* ------------------------------------------------------------------ */

void favourites_load(void) {
    s_favCount = 0;
    s_loaded   = true;

    char filePath[PATHNAME_MAX];
    if (!get_fav_path(filePath, sizeof(filePath))) return;

    FILE *f = fopen(filePath, "r");
    if (!f) return;

    char line[PATHNAME_MAX + FAV_DISPLAY_NAME + 4];
    while (fgets(line, sizeof(line), f) && s_favCount < MAX_FAVOURITES) {
        char *sep = strchr(line, '|');
        if (!sep) continue;
        *sep = '\0';
        char *name_part = sep + 1;

        /* Quitar \r\n */
        size_t nlen = strlen(name_part);
        if (nlen && name_part[nlen-1] == '\n') name_part[--nlen] = '\0';
        if (nlen && name_part[nlen-1] == '\r') name_part[--nlen] = '\0';

        if (!strlen(line) || !nlen) continue;

        strncpy(s_favList[s_favCount].path, line,      PATHNAME_MAX    -1);
        strncpy(s_favList[s_favCount].name, name_part, FAV_DISPLAY_NAME-1);
        s_favList[s_favCount].path[PATHNAME_MAX    -1] = '\0';
        s_favList[s_favCount].name[FAV_DISPLAY_NAME-1] = '\0';
        s_favCount++;
    }
    fclose(f);
}

void favourites_save(void) {
    char filePath[PATHNAME_MAX];
    if (!get_fav_path(filePath, sizeof(filePath))) return;

    ensure_path(DEVICE_CONFIG, "swiss/settings", NULL, false);

    FILE *f = fopen(filePath, "w");
    if (!f) return;
    for (int i = 0; i < s_favCount; i++)
        fprintf(f, "%s|%s\n", s_favList[i].path, s_favList[i].name);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/*  CRUD                                                               */
/* ------------------------------------------------------------------ */

bool favourites_contains(const char *path) {
    for (int i = 0; i < s_favCount; i++)
        if (!strcmp(s_favList[i].path, path)) return true;
    return false;
}

bool favourites_add(const char *path, const char *name) {
    if (!s_loaded) favourites_load();
    if (s_favCount >= MAX_FAVOURITES) return false;
    if (favourites_contains(path))    return false;

    strncpy(s_favList[s_favCount].path, path, PATHNAME_MAX    -1);
    strncpy(s_favList[s_favCount].name, name, FAV_DISPLAY_NAME-1);
    s_favList[s_favCount].path[PATHNAME_MAX    -1] = '\0';
    s_favList[s_favCount].name[FAV_DISPLAY_NAME-1] = '\0';
    s_favCount++;
    favourites_save();
    return true;
}

bool favourites_remove(const char *path) {
    if (!s_loaded) favourites_load();
    for (int i = 0; i < s_favCount; i++) {
        if (!strcmp(s_favList[i].path, path)) {
            memmove(&s_favList[i], &s_favList[i+1],
                    (s_favCount-i-1) * sizeof(FavouriteEntry));
            s_favCount--;
            favourites_save();
            return true;
        }
    }
    return false;
}

FavouriteEntry *favourites_get_list(void)  { return s_favList;  }
int             favourites_get_count(void) { return s_favCount; }

/* ------------------------------------------------------------------ */
/*  UI — patrón CORRECTO de Swiss (igual a select_recent_entry)        */
/*                                                                      */
/*  Controles:                                                          */
/*    Arriba/Abajo / Stick → navegar                                    */
/*    A → abrir favorito                                                */
/*    X → eliminar favorito                                             */
/*    B → cerrar                                                        */
/* ------------------------------------------------------------------ */

void favourites_show_ui(void) {
    if (!s_loaded) favourites_load();

    if (s_favCount == 0) {
        uiDrawObj_t *msg = DrawPublish(DrawMessageBox(D_INFO,
            "Sin favoritos.\n"
            "Presiona Y sobre un juego para agregar."));
        wait_press_A();
        DrawDispose(msg);
        return;
    }

    int idx = 0;
    int rh  = 22;
    int fileListBase = 175;
    uiDrawObj_t *container = NULL;

    while (1) {
        int max = s_favCount;
        if (idx >= max) idx = max - 1;
        if (idx < 0)    idx = 0;

        /* Reconstruir panel completo — patrón correcto de Swiss */
        uiDrawObj_t *newPanel = DrawEmptyBox(
            30, fileListBase - 40,
            getVideoMode()->fbWidth - 30,
            fileListBase + (max * (rh + 2)) + 20);

        DrawAddChild(newPanel,
            DrawLabel(45, fileListBase - 28, "Favoritos:"));
        DrawAddChild(newPanel,
            DrawLabel(45, fileListBase - 14,
                      "[A] Abrir  [X] Borrar  [B] Cerrar"));

        for (int i = 0; i < max; i++) {
            DrawAddChild(newPanel,
                DrawSelectableButton(
                    45,
                    fileListBase + i*(rh+2),
                    getVideoMode()->fbWidth - 45,
                    fileListBase + i*(rh+2) + rh,
                    s_favList[i].name,
                    (i == idx) ? B_SELECTED : B_NOSELECT));
        }

        if (container) DrawDispose(container);
        DrawPublish(newPanel);
        container = newPanel;

        /* Esperar input — mismo patrón que select_recent_entry */
        while ((padsStickY() > -16 && padsStickY() < 16) &&
               !(padsButtonsHeld() & (PAD_BUTTON_B | PAD_BUTTON_A |
                                      PAD_BUTTON_X | PAD_BUTTON_UP |
                                      PAD_BUTTON_DOWN)))
        { VIDEO_WaitVSync(); }

        if ((padsButtonsHeld() & PAD_BUTTON_UP) || padsStickY() > 16)
            idx = (--idx < 0) ? max - 1 : idx;
        if ((padsButtonsHeld() & PAD_BUTTON_DOWN) || padsStickY() < -16)
            idx = (idx + 1) % max;

        if (padsButtonsHeld() & PAD_BUTTON_A) {
            while (padsButtonsHeld() & PAD_BUTTON_A) VIDEO_WaitVSync();
            break;
        }

        if (padsButtonsHeld() & PAD_BUTTON_X) {
            favourites_remove(s_favList[idx].path);
            while (padsButtonsHeld() & PAD_BUTTON_X) VIDEO_WaitVSync();
            if (s_favCount == 0) { idx = -1; break; }
            if (idx >= s_favCount) idx = s_favCount - 1;
            continue; /* redibuja lista actualizada */
        }

        if (padsButtonsHeld() & PAD_BUTTON_B) {
            idx = -1;
            break;
        }

        if (padsStickY() < -16 || padsStickY() > 16)
            usleep(50000 - abs(padsStickY() * 16));

        /* Debounce */
        while (padsButtonsHeld() & (PAD_BUTTON_B | PAD_BUTTON_A |
                                    PAD_BUTTON_X | PAD_BUTTON_UP |
                                    PAD_BUTTON_DOWN))
        { VIDEO_WaitVSync(); }
    }

    do { VIDEO_WaitVSync(); } while (padsButtonsHeld() & PAD_BUTTON_B);
    DrawDispose(container);

    if (idx >= 0 && idx < s_favCount) {
        int res = find_existing_entry(s_favList[idx].path, true);
        if (res) {
            uiDrawObj_t *err = DrawPublish(DrawMessageBox(D_FAIL,
                res == RECENT_ERR_ENT_MISSING
                    ? "Favorito no encontrado.\nPresiona A para continuar."
                    : "Dispositivo no disponible.\nPresiona A para continuar."));
            wait_press_A();
            DrawDispose(err);
        }
    }
}
