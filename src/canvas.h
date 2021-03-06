#ifndef _BURRO_CANVAS_H
#define _BURRO_CANVAS_H

#include <stdint.h>
#include <gtk/gtk.h>
#include "visibility.h"
#include "canvas_vram.h"
#include "canvas_audio.h"

#pragma GCC visibility push(default)
#define BURRO_TYPE_CANVAS (burro_canvas_get_type ())
G_DECLARE_FINAL_TYPE (BurroCanvas, burro_canvas, BURRO, CANVAS, GtkDrawingArea)
#pragma GCC visibility pop

#define CANVAS_WIDTH 512
#define CANVAS_HEIGHT 384
#define CANVAS_MARGIN 50

#define CANVAS_ZLEVEL_COUNT 4
BurroCanvas *burro_canvas_new ();

gboolean     canvas_xy_to_index (BurroCanvas *canvas, double x, double y, int *index, int *trailing);

void         canvas_init_guile_procedures ();

cairo_t * get_canvas_context_cur();

#endif
