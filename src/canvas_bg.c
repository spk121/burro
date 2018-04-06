/*  bg.c

    Copyright (C) 2018   Michael L. Gran
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

#include "canvas_bg.h"
#include "canvas_lib.h"

#define CANVAS_WIDTH 512
#define CANVAS_HEIGHT 384
#define CANVAS_MARGIN 50


/** Information about a single layer of a multi-layer background. */
typedef struct
{
    /** BG is displayed when true */
    gboolean enable;

    /** tile and map, or true color bmp */
    bg_type_t type;

    /** the "user" or screen location of the rotation center of the
     * background */
    double scroll_x, scroll_y;

    /** the "device" location of the rotation center of the
     * background */
    double rotation_center_x, rotation_center_y;

    /** the expansion factor of the background: 1.0 = 1 pixel per
     * pixel */
    double expansion;

    /** the rotation angle of the background about its rotation
     * center, in radians */
    double rotation;

    /** when true, there are changes to the background that need
     *  to be rendered. */
    double dirty;

    /** for BMP background, the VRAM store from which the BMP is
     * taken.  Form MAP backgrounds, the tilesheet. */
    vram_bank_t img_vram;

    /** for MAP backgrounds only, the VRAM store from which the
     * tilesheet data is taken. */
    vram_bank_t map_vram;

    /** For BMP background, the top-left corner of the VRAM-stored
     *  image that will be used in the background.  For MAP background
     *  the top-left corner of the VRAM-stored map that will be used. */
    int vram_i, vram_j;
    
    /** For BMP background, the number of rows and columns of the
     *  VRAM-stored image that will be used in the background.  For
     *  MAP background the number of rows and columns VRAM-stored map
     *  that will be used. */
    int vram_width, vram_height;
} bg_entry_t;

/** Information about all the layers of a multi-layer background. */
typedef struct
{
    /** When true, the colors of all the background layers have
        their red and blue swapped. */
    gboolean colorswap;

    /** Factor to adjust the brightness or darkness of the background.
        Default is 1.0.  When 0.0, all background colors are
        black.  */
    double brightness;

    /** Storage for info on the background layers */
    bg_entry_t bg[BG_BACKGROUNDS_COUNT];

    /** Cache for the Cairo renderings of background layers.  */
    cairo_surface_t *surf[BG_BACKGROUNDS_COUNT];
} bg_t;

/** Static storage for all the background layers and their Cairo
 * renderings.  */
bg_t bg;

const char
bg_index_name[BG_3 + 1][11] = {
    [BG_0] = "BG_0",
    [BG_1] = "BG_1",
    [BG_2] = "BG_2",
    [BG_3] = "BG_3",
};


////////////////////////////////////////////////////////////////

static cairo_surface_t *
bg_render_to_cairo_surface (bg_index_t id);
/// static cairo_surface_t *
// bg_render_map_to_cairo_surface (bg_index_t id);
static cairo_surface_t *
bg_render_bmp_to_cairo_surface (bg_index_t id);
// static void
// bg_update (bg_index_t id);


////////////////

static gboolean
bg_validate_int_as_bg_index_t (int x)
{
    return (x >= (int) BG_0 && x <= (int) BG_3);
}

static gboolean
bg_validate_int_as_bg_type_t (int x)
{
    return (x >= (int) BG_TYPE_NONE && x <= (int) BG_TYPE_BMP);
}

static gboolean
bg_validate_bg_index_t (bg_index_t x)
{
    return (x >= BG_0 && x <= BG_3);
}

static const char *
bg_get_index_name (bg_index_t index)
{
    g_assert (bg_validate_bg_index_t (index));
    return bg_index_name[index];
}

void
canvas_bg_init ()
{
    bg.colorswap = FALSE;
    bg.brightness = 1.0;
    for (int i = 0; i < BG_BACKGROUNDS_COUNT; i ++)
    {
        bg.bg[i].enable = FALSE;
        bg.bg[i].type = BG_TYPE_NONE;
        bg.bg[i].scroll_x = 0.0;
        bg.bg[i].scroll_y = 0.0;
        bg.bg[i].rotation_center_x = 0.0;
        bg.bg[i].rotation_center_y = 0.0;
        bg.bg[i].expansion = 1.0;
        bg.bg[i].rotation = 0.0;
        bg.bg[i].dirty = FALSE;
        bg.bg[i].map_vram = VRAM_NONE;
        bg.bg[i].img_vram = VRAM_NONE;
        bg.bg[i].vram_i = 0;
        bg.bg[i].vram_j = 0;
        bg.bg[i].vram_width = 0;
        bg.bg[i].vram_height = 0;
        bg.surf[i] = NULL;
    }
}

cairo_surface_t *
canvas_bg_get_cairo_surface (bg_index_t id)
{
    g_assert (bg.bg[id].type != BG_TYPE_NONE);
    g_assert (bg.surf[id] != NULL);
    g_assert (cairo_surface_get_reference_count (bg.surf[id]) > 0);
    g_assert (cairo_surface_get_reference_count (bg.surf[id]) < 10);

    return bg.surf[id];
}


gboolean
canvas_bg_is_dirty (bg_index_t z)
{
    return bg.bg[z].dirty;
}

gboolean
canvas_bg_set_clean (bg_index_t z)
{
    bg.bg[z].dirty = FALSE;
}

void
canvas_bg_fini (void)
{
    for (int id = 0; id < BG_BACKGROUNDS_COUNT; id ++)
    {
        if (bg.surf[id] != NULL)
            cairo_surface_destroy (bg.surf[id]);
        bg.surf[id] = NULL;
    }
}

#if 0
void bg_assign_memory (bg_index_t id, matrix_size_t siz, vram_bank_t bank)
{
    matrix_attach_to_vram (siz, bank, &(bg.bg[id].storage), &(bg.bg[id].data));
    bg.bg[id].size = siz;
    bg.bg[id].bank = bank;
}
#endif

/** Apply the background colorswap and brightness properties to an ARGB32
 *  colorval.
 *  @param [in] c32 - original color
 *  @return modified colorval
 */
static uint32_t
adjust_colorval (uint32_t c32)
{
    uint32_t a, r, g, b;

    if (bg.brightness == 1.0 && bg.colorswap == FALSE)
        return c32;

    a = (((uint32_t) c32 & 0xff000000) >> 24);
    r = (((uint32_t) c32 & 0x00ff0000) >> 16);
    g = (((uint32_t) c32 & 0x0000ff00) >> 8);
    b = ((uint32_t) c32 & 0x000000ff);

    if (bg.colorswap)
    {
        uint32_t temp = r;
        r = b;
        b = temp;
    }
    r = r * bg.brightness;
    g = g * bg.brightness;
    b = b * bg.brightness;
    if (r > 0xFF) r = 0xFF;
    if (g > 0xFF) g = 0xFF;
    if (b > 0xFF) b = 0xFF;
    c32 = (a << 24) + (r << 16) + (g << 8) + b;
    return c32;
}

gboolean
canvas_bg_is_shown (bg_index_t id)
{
    return bg.bg[id].enable;
}

static void
bg_hide (bg_index_t id)
{
    bg.bg[id].enable = FALSE;
}

static void
bg_reset (bg_index_t id)
{
    bg.bg[id].enable = FALSE;
    bg.bg[id].type = BG_TYPE_NONE;
    bg.bg[id].scroll_x = 0.0;
    bg.bg[id].scroll_y = 0.0;
    bg.bg[id].rotation_center_x = 0.0;
    bg.bg[id].rotation_center_y = 0.0;
    bg.bg[id].expansion = 1.0;
    bg.bg[id].rotation = 0.0;
}

static void
bg_rotate (bg_index_t id, double angle)
{
    bg.bg[id].rotation += angle;
}

static void
bg_scroll (bg_index_t id, double dx, double dy)
{
    bg.bg[id].scroll_x += dx;
    bg.bg[id].scroll_y += dy;
}

static void
bg_set (bg_index_t id, double rotation, double expansion,
        double scroll_x, double scroll_y,
        double rotation_center_x, double rotation_center_y)
{
    bg.bg[id].rotation = rotation;
    bg.bg[id].expansion = expansion;
    bg.bg[id].scroll_x = scroll_x;
    bg.bg[id].scroll_y = scroll_y;
    bg.bg[id].rotation_center_x = rotation_center_x;
    bg.bg[id].rotation_center_y = rotation_center_y;
}

static void
bg_set_expansion (bg_index_t id, double expansion)
{
    bg.bg[id].expansion = expansion;
}

static void
bg_set_rotation (bg_index_t id, double rotation)
{
    bg.bg[id].rotation = rotation;
}

static void
bg_set_rotation_center (bg_index_t id,
                        double rotation_center_x, double rotation_center_y)
{
    bg.bg[id].rotation_center_x = rotation_center_x;
    bg.bg[id].rotation_center_y = rotation_center_y;
}

static void
bg_set_rotation_expansion (bg_index_t id, double rotation, double expansion)
{
    bg.bg[id].rotation = rotation;
    bg.bg[id].expansion = expansion;
}

static void
bg_show (bg_index_t id)
{
    g_assert (bg.bg[id].type != BG_TYPE_NONE);

    bg.bg[id].enable = TRUE;
}

static cairo_surface_t *
bg_render_to_cairo_surface (bg_index_t id)
{
    g_return_val_if_fail (id >= 0 && id < BG_BACKGROUNDS_COUNT, NULL);

    switch (bg.bg[id].type)
    {
    case BG_TYPE_NONE:
        return NULL;
        break;
    case BG_TYPE_BMP:
        return bg_render_bmp_to_cairo_surface (id);
        break;
    }
    g_return_val_if_reached (NULL);
}

static cairo_surface_t *
bg_render_bmp_to_cairo_surface (bg_index_t id)
{
    int width, height, stride;
    uint32_t *data;
    uint32_t c32;
    cairo_surface_t *surf;

    width = bg.bg[id].vram_width;
    height = bg.bg[id].vram_height;

    g_return_val_if_fail (width > 0 && height > 0, NULL);

    surf = xcairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    data = xcairo_image_surface_get_argb32_data (surf);
    stride = xcairo_image_surface_get_argb32_stride (surf);
    xcairo_surface_flush (surf);

    int v = bg.bg[id].img_vram;
    int vj, vi;
    
    for (int j = 0; j < height; j++)
    {
        vj = j + bg.bg[id].vram_j;
        for (int i = 0; i < width; i++)
        {
            vi = i + bg.bg[id].vram_i;
            c32 = vram_get_u32_ptr(v)[vj * vram_get_u32_width(v) + vi];
            data[j * stride + i] = adjust_colorval (c32);
        }
    }
    
    xcairo_surface_mark_dirty (surf);
    return surf;
}

void
canvas_bg_get_transform (bg_index_t id, double *scroll_x, double *scroll_y,
                         double *rotation_center_x, double *rotation_center_y,
                         double *rotation, double *expansion)
{
    *scroll_x = bg.bg[id].scroll_x;
    *scroll_y = bg.bg[id].scroll_y;
    *rotation_center_x = bg.bg[id].rotation_center_x;
    *rotation_center_y = bg.bg[id].rotation_center_y;
    *rotation = bg.bg[id].rotation;
    *expansion = bg.bg[id].expansion;
}

////////////////////////////////////////////////////////////////

static SCM
_scm_from_bg_index_t (bg_index_t x)
{
    return scm_from_int ((int) x);
}

static bg_index_t
_scm_to_bg_index_t (SCM x)
{
    return (bg_index_t) scm_to_int (x);
}

static gboolean
_scm_is_bg_index_t (SCM x)
{
    return scm_is_integer(x) && bg_validate_int_as_bg_index_t (scm_to_int (x));
}

#if 0
SCM_DEFINE (G_bg_assign_memory, "bg-assign-memory", 3, 0, 0, (SCM id, SCM matrix_size, SCM vram_bank), "\
Assign a memory location and a VRAM bank to a BG layer.")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-assign-memory");
    SCM_ASSERT(_scm_is_matrix_size_t(matrix_size), matrix_size, SCM_ARG2, "bg-assign-memory");
    SCM_ASSERT(_scm_is_vram_bank_t(vram_bank), vram_bank, SCM_ARG3, "bg-assign-memory");

    // FIXME: throw errors is VRAM bank is used or if matrix and vram sizes
    // don't make sense.
    matrix_size_t c_matrix_size = _scm_to_matrix_size_t (matrix_size);
    vram_bank_t c_vram_bank = _scm_to_vram_bank_t (vram_bank);

    if (matrix_get_u32_size(c_matrix_size) > vram_get_u32_size(c_vram_bank))
        guile_vram_error ("bg-assign-memory", c_vram_bank);

    bg_assign_memory(_scm_to_bg_index_t(id), c_matrix_size, c_vram_bank);

    return SCM_UNSPECIFIED;
}
#endif
#if 0
SCM_DEFINE (G_bg_dump, "bg-dump", 1, 0, 0, (SCM sid), "\
Print to the console information about a background.")
{
    bg_index_t id = _scm_to_bg_index_t (sid);
    char str[80];
    sprintf(str, "BG %s %dx%d %s %s %s  priority %d",
            bg_get_index_name(id),
            matrix_get_width(bg.bg[id].size),
            matrix_get_height(bg.bg[id].size),
            bg.bg[id].type == BG_TYPE_MAP ? "MAP" :
            (bg.bg[id].type == BG_TYPE_BMP ? "BMP" : "NONE"),
            (bg.bg[id].enable ? "SHOWN" : "HIDDEN"),
            vram_get_bank_name(bg.bg[id].bank),
            bg.bg[id].priority);
    console_write_latin1_string(str);
    console_move_down(1);
    console_move_to_column(0);
    sprintf(str,"X %4.1f, Y %4.1f, ROTX %4.1f, ROTY %4.1f",
            bg.bg[id].scroll_x, bg.bg[id].scroll_y,
            bg.bg[id].rotation_center_x, bg.bg[id].rotation_center_y);
    console_write_latin1_string(str);
    console_move_down(1);
    console_move_to_column(0);
    sprintf(str,"expansion %4.1f, rotation %4.1f",
            bg.bg[id].expansion, bg.bg[id].rotation);
    console_write_latin1_string(str);
    console_move_down(1);
    console_move_to_column(0);

    return SCM_UNSPECIFIED;

    // enable, type, priority, scroll_x, scroll_y
    // rotation_center_x, rotatioN_center_y
    // expansion, rotation
    // matrix_size, vram_bank
    // name of last resource assignment
}
#endif

#if 0
SCM_DEFINE (G_bg_get_memory_assignment, "bg-dump-memory-assignment", 0, 0, 0, (void), "\
Returns the matrix size and VRAM bank of the BG layer.")
{
    for (bg_index_t i = BG_0; i <= BG_3; i++)
    {
        char *c_str = g_strdup_printf("%s %dx%d %s",
                                      bg_get_index_name(i),
                                      matrix_get_width(bg.bg[i].size),
                                      matrix_get_height(bg.bg[i].size),
                                      vram_get_bank_name(bg.bg[i].bank));
        console_write_latin1_string(c_str);
        console_move_down(1);
        console_move_to_column(0);
    }
    return SCM_UNSPECIFIED;
}
#endif

SCM_DEFINE (G_bg_hide, "bg-hide", 1, 0, 0, (SCM id), "\
Set background later ID to not be drawn")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-hide");
    bg_hide (scm_to_int (id));
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_bg_rotate, "bg-rotate", 2, 0, 0, (SCM id, SCM angle), "\
Rotate the background about its rotation center")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-rotate");
    bg_rotate (scm_to_int (id), scm_to_double (angle));
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_bg_scroll, "bg-scroll", 3, 0, 0, (SCM id, SCM dx, SCM dy), "\
Move the background")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-scroll");
    bg_scroll (scm_to_int (id), scm_to_double (dx), scm_to_double (dy));
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_bg_set, "bg-set", 7, 0, 0,
            (SCM id, SCM rotation, SCM expansion, SCM scroll_x, SCM scroll_y, SCM center_x, SCM center_y), "\
Set the position, rotation, expansion, and rotation center of a background.")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-set");

    bg_set (scm_to_int(id), scm_to_double(rotation), scm_to_double (expansion), scm_to_double (scroll_x),
            scm_to_double (scroll_y), scm_to_double (center_x), scm_to_double (center_y));
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_bg_set_expansion, "bg-set-expansion", 2, 0, 0, (SCM id, SCM expansion), "\
Set BG expansion")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-set-expansion");

    bg_set_expansion (scm_to_int (id), scm_to_double (expansion));
    return SCM_UNSPECIFIED;
}


SCM_DEFINE (G_bg_set_rotation, "bg-set-rotation", 2, 0, 0, (SCM id, SCM rotation), "\
Set BG rotation")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-set-rotation");

    bg_set_rotation (scm_to_int (id), scm_to_double (rotation));
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_bg_set_rotation_center, "bg-set-rotation-center", 3, 0, 0, (SCM id, SCM x, SCM y), "\
Move the rotation center of the background")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-set-rotation-center");

    bg_set_rotation_center (scm_to_int (id), scm_to_double (x), scm_to_double (y));
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_bg_set_rotation_expansion, "bg-set-rotation-expansion", 3, 0, 0, (SCM id, SCM r, SCM e), "\
Set the rotation angle and expansion of a BG")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-set-rotation-expansion");

    bg_set_rotation_expansion (scm_to_int (id), scm_to_double (r), scm_to_double (e));
    return SCM_UNSPECIFIED;
}

#if 0
SCM_DEFINE (G_bg_show, "bg-show", 1, 0, 0, (SCM id), "\
Set background later ID to be drawn")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-show");

    bg_index_t c_id = _scm_to_bg_index_t (id);
    if (bg.bg[c_id].type == BG_TYPE_NONE)
        guile_show_unassigned_bg_error ("bg-show", c_id);

    bg_show (scm_to_int (id));
    return SCM_UNSPECIFIED;
}
#endif

SCM_DEFINE (G_bg_shown_p, "bg-shown?", 1, 0, 0, (SCM id), "\
Return #t if indicated background layer is visible.")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-shown?");

    return scm_from_bool (canvas_bg_is_shown (scm_to_int (id)));
}

#if 0
SCM_DEFINE (G_bg_update, "bg-update", 1, 0, 0, (SCM id), "\
Apply all changes to this background layer since the last call to 'bg-update'")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-update");
    bg_update (scm_to_int (id));
    return SCM_UNSPECIFIED;
}
#endif

#if 0
SCM_DEFINE (G_bg_modify, "bg-get-bytevector", 1, 0, 0, (SCM id), "\
Returns a bytevector of data that holds the BG bitmap or map data")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-get-bytevector");

    // First make a pointer
    bg_index_t i = scm_to_int (id);
    SCM pointer = scm_from_pointer (bg_get_data_ptr (i), NULL);

    // Then make a bytevector
    SCM len = scm_from_size_t (matrix_get_u32_size (bg.bg[i].size));
    SCM zero_offset = scm_from_size_t (0);
    SCM uvec_type = scm_from_locale_symbol("u32");

    return scm_pointer_to_bytevector (pointer, len, zero_offset, uvec_type);
}
#endif

#if 0
SCM_DEFINE (G_bg_get_dimensions,"bg-get-dimensions", 1, 0, 0, (SCM id), "")
{
    SCM_ASSERT(_scm_is_bg_index_t(id), id, SCM_ARG1, "bg-get-dimensions");

    matrix_size_t siz = bg.bg[scm_to_int(id)].size;
    return scm_list_3 (scm_from_int (matrix_get_width (siz)),
                       scm_from_int (matrix_get_height (siz)),
                       scm_from_int (matrix_get_u32_size(siz)));
}
#endif

SCM_DEFINE(G_bg_get_colorswap, "bg-get-colorswap", 0, 0, 0, (void), "\
return the status of the colorswap flag for backgrounds.")
{
    return scm_from_bool(bg.colorswap);
}

SCM_DEFINE(G_bg_set_colorswap, "bg-set-colorswap", 1, 0, 0, (SCM flag), "\
set the colorswap boolean flag for backgrounds.")
{
    if (scm_is_true(flag))
        bg.colorswap = TRUE;
    else if (scm_is_false(flag))
        bg.colorswap = FALSE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE(G_bg_get_brightness, "bg-get-brightness", 0, 0, 0, (void), "\
Return the brightness factor for backgrounds.")
{
    return scm_from_double(bg.brightness);
}

SCM_DEFINE(G_bg_set_brightness, "bg-set-brightness", 1, 0, 0, (SCM x), "\
Adjusts the brightness of all the colors used in backgrounds. Usually \n\
a value between 0.0 (black) and 1.0 (unmodified colors).")
{
    bg.brightness = scm_to_double(x);
    for (int i = 0; i < 4; i ++)
    {
        if (bg.bg[i].enable)
        {
            bg.surf[i] = bg_render_to_cairo_surface(i);
            bg.bg[i].dirty = TRUE;
        }
    }
    
    return SCM_UNSPECIFIED;
}


SCM_VARIABLE_INIT (G_BG_TYPE_BMP, "BG_TYPE_BMP", scm_from_int (BG_TYPE_BMP));

SCM_VARIABLE_INIT (G_BG_0, "BG_0", scm_from_int (BG_0));
SCM_VARIABLE_INIT (G_BG_1, "BG_1", scm_from_int (BG_1));
SCM_VARIABLE_INIT (G_BG_2, "BG_2", scm_from_int (BG_2));
SCM_VARIABLE_INIT (G_BG_3, "BG_3", scm_from_int (BG_3));

SCM_DEFINE (G_set_background_image, "bg-setup", 2, 4, 0,
            (SCM s_id, SCM s_vram, SCM s_i, SCM s_j, SCM s_width, SCM s_height), "")
{
    SCM_ASSERT (scm_is_signed_integer (s_id, BG_0, BG_3), s_id, SCM_ARG1, "set-background-image");
    SCM_ASSERT (scm_is_signed_integer (s_vram, VRAM_A, VRAM_J), s_vram, SCM_ARG2, "set-background-image");

    int id = scm_to_int (s_id);
    int vram = scm_to_int (s_vram);
    if (vram_get_type (vram == VRAM_TYPE_IMAGE))
    {
        bg.bg[id].enable = TRUE;
        bg.bg[id].type = BG_TYPE_BMP;
        bg.bg[id].scroll_x = 0;
        bg.bg[id].scroll_y = 0;
        bg.bg[id].rotation_center_x = 0;
        bg.bg[id].rotation_center_y = 0;
        bg.bg[id].expansion = 1.0;
        bg.bg[id].rotation = 0.0;
        bg.bg[id].img_vram = vram;
        if (SCM_UNBNDP (s_i))
            bg.bg[id].vram_i = 0;
        else
            bg.bg[id].vram_i = scm_to_int (s_i);
        if (SCM_UNBNDP (s_j))
            bg.bg[id].vram_j = 0;
        else
            bg.bg[id].vram_j = scm_to_int (s_j);
        if (SCM_UNBNDP (s_width))
            bg.bg[id].vram_width = MIN(CANVAS_WIDTH, vram_get_width(vram));
        else
            bg.bg[id].vram_width = MIN(CANVAS_WIDTH, scm_to_int (s_width));
        if (SCM_UNBNDP (s_height))
            bg.bg[id].vram_height = MIN(CANVAS_HEIGHT, vram_get_height(vram));
        else
            bg.bg[id].vram_height = MIN(CANVAS_HEIGHT, scm_to_int (s_height));
    }
    bg.surf[id] = bg_render_to_cairo_surface(id);
    bg.bg[id].dirty = TRUE;
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_move_background, "bg-move", 3, 4, 0,
            (SCM id, SCM scroll_x, SCM scroll_y, SCM rotation, SCM expansion,
             SCM center_x, SCM center_y), "")
{
    SCM_ASSERT (scm_is_signed_integer (id, BG_0, BG_3), id, SCM_ARG1, "set-background-image");

    int i = scm_to_int (id);
    bg.bg[i].scroll_x = scm_to_double (scroll_x);
    bg.bg[i].scroll_y = scm_to_double (scroll_y);
    if (!SCM_UNBNDP(center_x))
        bg.bg[i].rotation_center_x = scm_to_double (center_x);
    if (!SCM_UNBNDP(center_y))
        bg.bg[i].rotation_center_y = scm_to_double (center_y);
    if (!SCM_UNBNDP(expansion))
        bg.bg[i].expansion = scm_to_double (expansion);
    if (!SCM_UNBNDP(rotation))
        bg.bg[i].rotation = scm_to_double (rotation);
    return SCM_UNSPECIFIED;
}

SCM_DEFINE (G_reset_background, "bg-reset", 1, 0, 0,
            (SCM id), "")
{
    SCM_ASSERT (scm_is_signed_integer (id, BG_0, BG_3), id, SCM_ARG1, "set-background-image");
    
    int i = scm_to_int (id);
    bg_reset(i);
        
    return SCM_UNSPECIFIED;    
}

void
canvas_bg_init_guile_procedures (void)
{
#ifndef SCM_MAGIC_SNARFER
#include "canvas_bg.x"
#endif
    scm_c_export ("bg-setup",
                  "bg-move",
                  "bg-reset",
                  "bg-set-colorswap",
                  "bg-get-colorswap",
                  "bg-set-brightness",
                  "bg-get-brightness",
                  "bg-hide",
                  "bg-show",
                  "bg-rotate",
                  "bg-scroll",
                  "bg-set-expansion",
                  "bg-set-rotation",
                  "bg-set-rotation-center",
                  "bg-set-rotation-expansion",
                  
                  "BG_0",
                  "BG_1",
                  "BG_2",
                  "BG_3",
                  NULL
        );
                  

#if 0

        "bg-assign-memory",
                  "bg-dump-memory-assignment",
                  "bg-dump",

                  "bg-reset",


                  "bg-set",



                  "bg-update",

                  "bg-get-width",
                  "bg-get-height",
                  "bg-get-u32-size",
                  "bg-get-bytevector",
                  // "bg->list-of-bytevectors",


                  "BG_TYPE_BMP",
                  "BG_TYPE_MAP",
                  NULL
#endif
}

/*
  Local Variables:
  mode:C
  c-file-style:"linux"
  tab-width:4
  c-basic-offset: 4
  indent-tabs-mode:nil
  End:
*/
