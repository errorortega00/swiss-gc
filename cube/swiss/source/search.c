/* search.c — v2 (corregido)
 *
 * BUGS CORREGIDOS vs v1:
 *   - padsCStickX() no existe → usa DrawGetTextEntry() que es el
 *     teclado virtual nativo de Swiss (ENTRYMODE_ALPHA | ENTRYMODE_FILE)
 *   - DrawTransparentBox() retorna uiDrawObj_t* — se publica correctamente
 *   - El bloque "continue" con leak de memoria fue eliminado
 *   - search_apply_filter retorna puntero seguro: nunca devuelve el
 *     array original, siempre asigna uno nuevo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <gccore.h>
#include "search.h"
#include "gui/FrameBufferMagic.h"
#include "gui/IPLFontWrite.h"
#include "swiss.h"
#include "main.h"

/* ------------------------------------------------------------------ */
/*  Estado interno                                                      */
/* ------------------------------------------------------------------ */

static bool s_active                       = false;
static char s_query[SEARCH_MAX_LEN + 1]   = {0};

/* ------------------------------------------------------------------ */
/*  API pública                                                         */
/* ------------------------------------------------------------------ */

bool        search_is_active(void)    { return s_active; }
const char *search_get_query(void)    { return s_query;  }

void search_clear(void) {
    s_active = false;
    memset(s_query, 0, sizeof(s_query));
}

/*
 * Abre el teclado virtual de Swiss para editar el query de búsqueda.
 * Usa DrawGetTextEntry() que es el sistema nativo (d-pad + A/B).
 * Si el usuario confirma con Start → activa el filtro.
 * Si cancela con B → desactiva y limpia.
 */
void search_open_keyboard(void) {
    char buf[SEARCH_MAX_LEN + 1];
    strncpy(buf, s_query, SEARCH_MAX_LEN);
    buf[SEARCH_MAX_LEN] = '\0';

    /* DrawGetTextEntry modifica buf in-place.
     * El usuario confirma con Start, cancela con B. */
    DrawGetTextEntry(ENTRYMODE_ALPHA | ENTRYMODE_FILE,
                     "Buscar archivo:", buf, SEARCH_MAX_LEN);

    if (strlen(buf) > 0) {
        strncpy(s_query, buf, SEARCH_MAX_LEN);
        s_query[SEARCH_MAX_LEN] = '\0';
        /* Convertir a minúsculas para búsqueda case-insensitive */
        for (int i = 0; s_query[i]; i++)
            s_query[i] = tolower((unsigned char)s_query[i]);
        s_active = true;
    } else {
        /* Buffer vacío → limpiar búsqueda */
        search_clear();
    }
}

/*
 * Aplica el filtro sobre el array ordenado.
 * SIEMPRE retorna un array nuevo en heap (caller hace free()).
 * Nunca retorna el puntero original para evitar doble free.
 * out_count = número de resultados.
 */
file_handle **search_apply_filter(file_handle **sorted, int count,
                                  int *out_count) {
    /* Siempre asignamos un array nuevo — seguro para free() del caller */
    file_handle **result = calloc(count > 0 ? count : 1,
                                  sizeof(file_handle *));
    *out_count = 0;

    if (!result) {
        /* Sin RAM: devolver count para que el caller pueda usar sorted */
        *out_count = count;
        return NULL;
    }

    if (!s_active || strlen(s_query) == 0) {
        /* Sin filtro activo: copiar todos los punteros */
        memcpy(result, sorted, count * sizeof(file_handle *));
        *out_count = count;
        return result;
    }

    for (int i = 0; i < count; i++) {
        if (!sorted[i]) continue;

        /* Directorios siempre pasan (para poder navegar) */
        if (sorted[i]->fileType == IS_DIR) {
            result[(*out_count)++] = sorted[i];
            continue;
        }

        /* Comparar nombre en minúsculas */
        char lname[PATHNAME_MAX];
        strncpy(lname, sorted[i]->name, PATHNAME_MAX - 1);
        lname[PATHNAME_MAX - 1] = '\0';
        for (int j = 0; lname[j]; j++)
            lname[j] = tolower((unsigned char)lname[j]);

        if (strstr(lname, s_query))
            result[(*out_count)++] = sorted[i];
    }

    return result;
}

/*
 * Dibuja barra de estado de búsqueda encima del browser.
 * Se llama desde drawFiles() cuando search_is_active() == true.
 * Publica y devuelve el objeto para que el caller pueda liberarlo.
 */
uiDrawObj_t *search_draw_status(void) {
    if (!s_active) return NULL;

    GXRModeObj *vmode = getVideoMode();
    int cx  = vmode->fbWidth / 2;
    int x1  = cx - 180;
    int x2  = cx + 180;

    char label[64];
    snprintf(label, sizeof(label),
             strlen(s_query) > 0 ? "Buscar: \"%s\"" : "Buscar: (vacío)",
             s_query);

    uiDrawObj_t *container = DrawContainer();
    DrawAddChild(container, DrawTransparentBox(x1, 6, x2, 28));
    DrawAddChild(container, DrawStyledLabel(cx, 14, label,
                                            0.80f, ALIGN_CENTER, defaultColor));
    return DrawPublish(container);
}
