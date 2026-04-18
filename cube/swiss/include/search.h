/* search.h — v2 */
#ifndef SEARCH_H
#define SEARCH_H
#include <stdbool.h>
#include <gccore.h>
#include "devices/deviceHandler.h"
#include "gui/FrameBufferMagic.h"

#define SEARCH_MAX_LEN 32

bool          search_is_active(void);
const char   *search_get_query(void);
void          search_clear(void);
void          search_open_keyboard(void);
file_handle **search_apply_filter(file_handle **sorted, int count,
                                  int *out_count);
uiDrawObj_t  *search_draw_status(void);

#endif
