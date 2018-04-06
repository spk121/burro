#ifndef CANVAS_BG_H
#define CANVAS_BG_H
/*  bg.h

    Copyright (C) 2013, 2014, 2018   Michael L. Gran
    This file is part of Burro Engine

    Burro Engine is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Burro Engine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Burro Engine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gtk/gtk.h>
#include <libguile.h>
#include "visibility.h"

#include "canvas_vram.h"

/** The number of background layers for the main screen */
#define BG_BACKGROUNDS_COUNT 4

/** Enumeration of the 4 background layer IDs */
typedef enum {
    BG_0 = 0,
    BG_1 = 1,
    BG_2 = 2,
    BG_3 = 3,
} bg_index_t;


/** Allowed background types, used in bg_init */
typedef enum {
    BG_TYPE_NONE,
    BG_TYPE_MAP, 
    BG_TYPE_BMP,
} bg_type_t;

DLL_LOCAL void canvas_bg_init (void);
DLL_LOCAL cairo_surface_t *canvas_bg_get_cairo_surface (bg_index_t id);
DLL_LOCAL gboolean canvas_bg_is_shown (bg_index_t id);
DLL_LOCAL gboolean canvas_bg_is_dirty (bg_index_t id);
DLL_PUBLIC gboolean canvas_bg_set_clean (bg_index_t z);
DLL_LOCAL void canvas_bg_get_transform (bg_index_t id, double *scroll_x, double *scroll_y,
                       double *rotation_center_x, double *rotation_center_y,
                       double *rotation, double *expansion);
DLL_LOCAL void canvas_bg_init_guile_procedures (void);
DLL_LOCAL void canvas_bg_fini(void);


/*
  Local Variables:
  mode:C
  c-file-style:"linux"
  tab-width:4
  c-basic-offset: 4
  indent-tabs-mode:nil
  End:
*/


#endif
