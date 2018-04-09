#include "canvas_vram.h"
#include "canvas_lib.h"

/*  vram.c

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

#include "canvas_vram.h"
#include "canvas_lib.h"

uint32_t vram_ABCD_store[VRAM_ABCD_U32_SIZE] __attribute__((aligned(32))) ;
uint32_t vram_EFGHIJ_store[VRAM_EFGHIJ_U32_SIZE] __attribute__((aligned(32))) ;

vram_info_t vram_info[VRAM_COUNT];

static GtkListStore *vram_list_store = NULL;

static const char vram_type_name[VRAM_N_TYPES][15] = {
    [VRAM_TYPE_RAW] = "Raw",
    [VRAM_TYPE_IMAGE] = "Image",
    [VRAM_TYPE_AUDIO] = "Audio"
};

static int vram_size[VRAM_COUNT] = {
    [VRAM_NONE] = VRAM_NONE_U32_SIZE,
    [VRAM_A] = VRAM_A_U32_SIZE,
    [VRAM_B] = VRAM_B_U32_SIZE,
    [VRAM_C] = VRAM_C_U32_SIZE,
    [VRAM_D] = VRAM_D_U32_SIZE,
    [VRAM_E] = VRAM_E_U32_SIZE,
    [VRAM_F] = VRAM_F_U32_SIZE,
    [VRAM_G] = VRAM_G_U32_SIZE,
    [VRAM_H] = VRAM_H_U32_SIZE,
    [VRAM_I] = VRAM_I_U32_SIZE,
    [VRAM_J] = VRAM_J_U32_SIZE,
};

static int vram_height[VRAM_COUNT] = {
    [VRAM_NONE] = VRAM_NONE_U32_HEIGHT,
    [VRAM_A] = VRAM_A_U32_HEIGHT,
    [VRAM_B] = VRAM_B_U32_HEIGHT,
    [VRAM_C] = VRAM_C_U32_HEIGHT,
    [VRAM_D] = VRAM_D_U32_HEIGHT,
    [VRAM_E] = VRAM_E_U32_HEIGHT,
    [VRAM_F] = VRAM_F_U32_HEIGHT,
    [VRAM_G] = VRAM_G_U32_HEIGHT,
    [VRAM_H] = VRAM_H_U32_HEIGHT,
    [VRAM_I] = VRAM_I_U32_HEIGHT,
    [VRAM_J] = VRAM_J_U32_HEIGHT,
};

static int vram_width[VRAM_COUNT] = {
    [VRAM_NONE] = VRAM_NONE_U32_WIDTH,
    [VRAM_A] = VRAM_A_U32_WIDTH,
    [VRAM_B] = VRAM_B_U32_WIDTH,
    [VRAM_C] = VRAM_C_U32_WIDTH,
    [VRAM_D] = VRAM_D_U32_WIDTH,
    [VRAM_E] = VRAM_E_U32_WIDTH,
    [VRAM_F] = VRAM_F_U32_WIDTH,
    [VRAM_G] = VRAM_G_U32_WIDTH,
    [VRAM_H] = VRAM_H_U32_WIDTH,
    [VRAM_I] = VRAM_I_U32_WIDTH,
    [VRAM_J] = VRAM_J_U32_WIDTH,
};

static uint32_t *vram_ptr[VRAM_COUNT] = {
    [VRAM_NONE] = VRAM_NONE_U32_PTR,
    [VRAM_A] = VRAM_A_U32_PTR,
    [VRAM_B] = VRAM_B_U32_PTR,
    [VRAM_C] = VRAM_C_U32_PTR,
    [VRAM_D] = VRAM_D_U32_PTR,
    [VRAM_E] = VRAM_E_U32_PTR,
    [VRAM_F] = VRAM_F_U32_PTR,
    [VRAM_G] = VRAM_G_U32_PTR,
    [VRAM_H] = VRAM_H_U32_PTR,
    [VRAM_I] = VRAM_I_U32_PTR,
    [VRAM_J] = VRAM_J_U32_PTR,
};

static const char 
vram_bank_name[VRAM_COUNT][10] = {
    [VRAM_NONE] = "VRAM_NONE",
    [VRAM_A] = "VRAM_A",
    [VRAM_B] = "VRAM_B",
    [VRAM_C] = "VRAM_C",
    [VRAM_D] = "VRAM_D",
    [VRAM_E] = "VRAM_E",
    [VRAM_F] = "VRAM_F",
    [VRAM_G] = "VRAM_G",
    [VRAM_H] = "VRAM_H",
    [VRAM_I] = "VRAM_I",
    [VRAM_J] = "VRAM_J",
};

static char *vram_path = NULL;

static gboolean
vram_validate_vram_bank_t (vram_bank_t x)
{
    return (x >= VRAM_A && x < VRAM_COUNT);
}


gboolean
vram_validate_int_as_vram_bank_t (int x)
{
    return (x >= (int) VRAM_A && x < (int) VRAM_COUNT);
}

void
vram_init (void)
{
    vram_zero_bank(VRAM_A);
    vram_zero_bank(VRAM_B);
    vram_zero_bank(VRAM_C);
    vram_zero_bank(VRAM_D);
    vram_zero_bank(VRAM_E);
    vram_zero_bank(VRAM_F);
    vram_zero_bank(VRAM_G);
    vram_zero_bank(VRAM_H);
    vram_zero_bank(VRAM_I);
    vram_zero_bank(VRAM_J);
}

const char *
vram_get_bank_name (vram_bank_t bank)
{
    g_assert (vram_validate_vram_bank_t (bank));
    return vram_bank_name[bank];
}

int
vram_get_u32_size (vram_bank_t bank)
{
    g_assert (vram_validate_vram_bank_t (bank));
    return vram_size[bank];
}

int
vram_get_u32_height (vram_bank_t bank)
{
    g_assert (vram_validate_vram_bank_t (bank));
    return vram_height[bank];
}
int
vram_get_u32_width (vram_bank_t bank)
{
    g_assert (vram_validate_vram_bank_t (bank));
    return vram_width[bank];
}

uint32_t *
vram_get_u32_ptr (vram_bank_t bank)
{
    g_assert (vram_validate_vram_bank_t (bank));
    return vram_ptr[bank];
}

void
vram_zero_bank (vram_bank_t bank)
{
    g_assert (vram_validate_vram_bank_t (bank));
    memset (vram_ptr[bank], 0, vram_size[bank] * sizeof(uint32_t));
}

void
canvas_vram_set_path (const char *path)
{
    free (vram_path);
    vram_path = NULL;
    if (path)
        vram_path = strdup(path);
}

static char *vram_size_string(int i)
{
    if (vram_info[i].type == VRAM_TYPE_RAW)
        return g_strdup_printf("%d kB", vram_size[i] * 4 / 1024);
    else if (vram_info[i].type == VRAM_TYPE_IMAGE)
        return g_strdup_printf("%d by %d", vram_info[i].width, vram_info[i].height);
    else if (vram_info[i].type == VRAM_TYPE_AUDIO)
        return g_strdup_printf("%d kB", vram_info[i].size / 1024);
    else
        return g_strdup ("unknown");
}

GtkListStore *vram_info_list_store_new()
{
    GtkListStore *list_store;
    GtkTreePath *path;
    GtkTreeIter iter;
    gint i;
    const char *nullstr = "";

    list_store = gtk_list_store_new (VRAM_N_COLUMNS,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING,
                                     G_TYPE_STRING);
    path = gtk_tree_path_new_from_string ("0");
    gtk_tree_model_get_iter(GTK_TREE_MODEL (list_store), &iter, path);
    gtk_tree_path_free (path);
    for (i = VRAM_A; i < VRAM_COUNT; i ++)
    {
        char *siz = vram_size_string(i);
        gtk_list_store_append (list_store, &iter);
        gtk_list_store_set (list_store, &iter,
                            VRAM_COLUMN_NAME, vram_bank_name[i],
                            VRAM_COLUMN_TYPE, vram_type_name[vram_info[i].type],
                            VRAM_COLUMN_FILENAME, (vram_info[i].filename
                                              ? vram_info[i].filename
                                              : nullstr),
                            VRAM_COLUMN_SIZE, siz,
                            -1);
        g_free (siz);
    }
    vram_list_store = list_store;
    return list_store;
}

void
vram_info_list_store_update(GtkListStore *list_store)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    gint i;
    const char *nullstr = "";

    for (i = VRAM_A; i < VRAM_COUNT; i ++)
    {
        path = gtk_tree_path_new_from_indices (i - VRAM_A, -1);
        gtk_tree_model_get_iter(GTK_TREE_MODEL (list_store), &iter, path);
        
        char *siz = vram_size_string(i);
        // gtk_list_store_append (list_store, &iter);
        gtk_list_store_set (list_store, &iter,
                            VRAM_COLUMN_NAME, vram_bank_name[i],
                            VRAM_COLUMN_TYPE, vram_type_name[vram_info[i].type],
                            VRAM_COLUMN_FILENAME, (vram_info[i].filename
                                              ? vram_info[i].filename
                                              : nullstr),
                            VRAM_COLUMN_SIZE, siz,
                            -1);
        g_free (siz);
    }
}


////////////////////////////////////////////////////////////////
SCM _scm_from_vram_bank_t (vram_bank_t x)
{
    return scm_from_int ((int) x);
}

vram_bank_t _scm_to_vram_bank_t (SCM x)
{
    return (vram_bank_t) scm_to_int (x);
}

gboolean _scm_is_vram_bank_t (SCM x)
{
    return scm_is_integer(x) && vram_validate_int_as_vram_bank_t (scm_to_int (x));
}

SCM_VARIABLE_INIT (G_VRAM_A, "VRAM_A", _scm_from_vram_bank_t (VRAM_A));
SCM_VARIABLE_INIT (G_VRAM_B, "VRAM_B", _scm_from_vram_bank_t (VRAM_B));
SCM_VARIABLE_INIT (G_VRAM_C, "VRAM_C", _scm_from_vram_bank_t (VRAM_C));
SCM_VARIABLE_INIT (G_VRAM_D, "VRAM_D", _scm_from_vram_bank_t (VRAM_D));
SCM_VARIABLE_INIT (G_VRAM_E, "VRAM_E", _scm_from_vram_bank_t (VRAM_E));
SCM_VARIABLE_INIT (G_VRAM_F, "VRAM_F", _scm_from_vram_bank_t (VRAM_F));
SCM_VARIABLE_INIT (G_VRAM_G, "VRAM_G", _scm_from_vram_bank_t (VRAM_G));
SCM_VARIABLE_INIT (G_VRAM_H, "VRAM_H", _scm_from_vram_bank_t (VRAM_H));
SCM_VARIABLE_INIT (G_VRAM_I, "VRAM_I", _scm_from_vram_bank_t (VRAM_I));
SCM_VARIABLE_INIT (G_VRAM_J, "VRAM_J", _scm_from_vram_bank_t (VRAM_J));
SCM_VARIABLE_INIT (G_VRAM_INDEX_LIST, "VRAM_INDEX_LIST",
                   scm_list_n(G_VRAM_A,
                              G_VRAM_B,
                              G_VRAM_C,
                              G_VRAM_D,
                              G_VRAM_E,
                              G_VRAM_F,
                              G_VRAM_G,
                              G_VRAM_H,
                              G_VRAM_I,
                              G_VRAM_J,
                              SCM_UNDEFINED));

int
vram_get_type (int z)
{
    return vram_info[z].type;
}

int
vram_get_width (int z)
{
    return vram_info[z].width;
}

int
vram_get_height (int z)
{
    return vram_info[z].height;
}

SCM_DEFINE (G_vram_get_type, "get-vram-type", 1, 0, 0, (SCM index), "\
Returns a symbol (none, or pixbuf) describing the current contents\n\
of the VRAM bank.")
{
    SCM_ASSERT(_scm_is_vram_bank_t(index), index, SCM_ARG1, "get-vram-type");
    vram_bank_t i = _scm_to_vram_bank_t (index);
    int type = vram_info[i].type;
    SCM ret;
    if (type == VRAM_TYPE_RAW)
        ret = scm_from_utf8_symbol ("raw");
    else if (type == VRAM_TYPE_IMAGE)
        ret = scm_from_utf8_symbol ("image");
    else if (type == VRAM_TYPE_AUDIO)
        ret = scm_from_utf8_symbol ("audio");
    else
        ret = scm_from_utf8_symbol ("unknown");

    return ret;
}

gboolean
canvas_vram_is_valid_filename (const char *filename)
{
    g_return_val_if_fail (filename != NULL && strlen(filename) > 0, FALSE);
    
    size_t len = strlen(filename);
    if (filename[len-1] == G_DIR_SEPARATOR)
        return FALSE;

    /* We only allow one level deep. */
    gchar *path = g_path_get_dirname(filename);
    int test = (!strcmp(path, ".")
                || (!strrchr(path, G_DIR_SEPARATOR)
                    && !strrchr(path, ':')
                    && strcmp (path, "..")));
    free(path);

    return test;
}

static gboolean
set_vram_to_pixbuf_from_image_file (int vram_index, const char *filename)
{
    g_return_val_if_fail (vram_index >= 0
                          && vram_index < VRAM_COUNT, FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    if (!canvas_vram_is_valid_filename (filename))
    {
        g_message ("Didn't load image file %s: filename is invalid", filename);
        return FALSE;
    }

    char *pathname;
    if (vram_path)
        pathname = g_build_filename (vram_path, filename, NULL);
    else
        pathname = strdup(filename);
    GdkPixbuf *pb = xgdk_pixbuf_new_from_file (pathname);
    g_free (pathname);
    if (pb == NULL)
        return FALSE;
  
    if (!xgdk_pixbuf_is_argb32 (pb) && !xgdk_pixbuf_is_xrgb32 (pb))
    {
        xg_object_unref (pb);
        g_critical ("%s is not a 24-bit or 32-bit color pixbuf", filename);
        return FALSE;
    }
    else
    {
        char *basename = g_path_get_basename (filename);
        int img_width, img_height, img_stride;
        gboolean opaque = xgdk_pixbuf_is_xrgb32 (pb) && !xgdk_pixbuf_is_argb32 (pb);
        if (opaque)
        {
            GdkPixbuf *pb2 = gdk_pixbuf_add_alpha (pb, FALSE, 0, 0, 0);
            g_object_unref(pb);
            pb = pb2;
        }

        xgdk_pixbuf_get_width_height_stride (pb, &img_width, &img_height, &img_stride);
        if (img_width > vram_get_u32_width (vram_index)
            || img_height > vram_get_u32_height (vram_index))
        {
            img_width = MIN(img_width, vram_get_u32_width (vram_index));
            img_height = MIN(img_height, vram_get_u32_height (vram_index));
            g_warning ("Image %s is too large for %s: only %d by %d pixels will be used.",
                    basename,
                    vram_bank_name[vram_index],
                    img_width, img_height);
        }
                    
        uint32_t *c32 = xgdk_pixbuf_get_argb32_pixels(pb);
        uint32_t *data = vram_get_u32_ptr (vram_index);
        for (int j = 0; j < img_height; j ++)
        {
            for (int i = 0; i < img_width; i ++)
            {
                // Convert from GDKPixbuf ABGR to Cairo ARGB
                uint32_t val = c32[j * img_stride + i];
                val = (val & 0xFF00FF00) | ((val >> 16) & 0xFF) | ((val & 0xFF) << 16);

                // Convert from GDK un-premultiplied alpha to Cairo pre-multiplied alpha
                unsigned a = val >> 24;
                unsigned r = (((val >> 16) & 0xFF) * a) / 256;
                unsigned g = (((val >> 8) & 0xFF) * a) / 256;
                unsigned b = (((val >> 0) & 0xFF) * a) / 256;
                data[j * vram_get_u32_width(vram_index) + i] = a << 24 | r << 16 | g << 8 | b;
            }
        }

        vram_info[vram_index].type = VRAM_TYPE_IMAGE;
        vram_info[vram_index].width = img_width;
        vram_info[vram_index].height = img_height;
        free (vram_info[vram_index].filename);
        vram_info[vram_index].filename = basename;
        
        g_info ("Loaded image %s into %s.", basename, vram_bank_name[vram_index]);
        g_object_unref (pb);
        return TRUE;
    }
    g_return_val_if_reached (TRUE);
}


////////////////////////////////////////////////////////////////
// Ogg Vorbis IO Callbacks

vram_io_context_t *
vram_audio_open (int index)
{
    if (!vram_validate_int_as_vram_bank_t (index)
        || vram_info[index].type != VRAM_TYPE_AUDIO
        || vram_info[index].size == 0)
        return NULL;
    
    vram_io_context_t *io = g_malloc (sizeof(vram_io_context_t));
    io->index = index;
    io->open = TRUE;
    io->ptr = vram_get_u32_ptr(io->index);
    io->size = vram_info[io->index].size;
    io->position = 0L;
    return io;
}

size_t
vram_audio_read (void *ptr, size_t size, size_t nmemb, void *context)
{
    vram_io_context_t *io = (vram_io_context_t *) context;
    long start, end;

    if (ptr == NULL || size == 0 || nmemb == 0)
        return 0;
    
    if (!io || !io->open)
        return 0;
    
    start = io->position;
    end = start + size * nmemb;
    if (start >= io->size)
        return 0;

    if (end > io->size)
        end = io->size;

    memmove (ptr, io->ptr + start, end - start);
    io->position = end;
    return end - start;
}

int
vram_audio_seek (void *context, ogg_int64_t offset, int whence)
{
    vram_io_context_t *io = (vram_io_context_t *) context;
    gint64 start = 0;
    gint64 end;

    if (!io || !io->open)
        return -1;

    if (whence == SEEK_CUR)
        start = io->position;
    else if (whence == SEEK_END)
        start = io->size;

    end = start + offset;
    if (end < 0)
        end = 0;
    if (end > io->size)
        end = io->size;
    io->position = end;
    if (end < G_MAXINT)
        return end;
    else
        return G_MAXINT;
}

int
vram_audio_close (void *context)
{
    vram_io_context_t *io = (vram_io_context_t *) context;
    io->open = FALSE;
    return 0;
}

long
vram_audio_tell (void *context)
{
    vram_io_context_t *io = (vram_io_context_t *) context;
    if (!io || !io->open)
        return -1;

    return io->position;
}
    
////////////////////////////////////////////////////////////////

SCM_DEFINE (G_vram_get_filename, "get-vram-filename", 1, 0, 0, (SCM index), "\
Returns, as a sring, the filename associated with the current contents\n\
of the VRAM bank.")
{
    SCM_ASSERT(_scm_is_vram_bank_t(index), index, SCM_ARG1, "get-vram-type");
    vram_bank_t i = _scm_to_vram_bank_t (index);
    char *fname = vram_info[i].filename;
    SCM ret;
    if (fname)
        ret =  scm_from_locale_string (fname);
    else
        ret = SCM_BOOL_F;

    return ret;
}

SCM_DEFINE (G_get_vram_image_size, "get-vram-image-size",
            1, 0, 0, (SCM index), "\
Returns, as a two-element list, the width and height of the image loaded\n\
in the VRAM bank.")
{
    SCM_ASSERT(_scm_is_vram_bank_t(index), index, SCM_ARG1, "get-vram-image-size");
    vram_bank_t i = _scm_to_vram_bank_t (index);
    SCM ret;
    if (vram_info[i].type == VRAM_TYPE_IMAGE)
        ret =  scm_list_2 (scm_from_int (vram_info[i].width),
                           scm_from_int (vram_info[i].height));
    else
        ret = SCM_BOOL_F;

    return ret;
}


SCM_DEFINE (G_vram_get_u32_size, "vram-get-u32-size", 1, 0, 0, (SCM index),
            "Given an index that represents a VRAM bank, return the maximum number of \n\
32-bit integers that it could contain in its bytevector.")
{
    SCM_ASSERT(_scm_is_vram_bank_t(index), index, SCM_ARG1, "vram-get-u32-size");
    
    return scm_from_int (vram_get_u32_size (_scm_to_vram_bank_t (index)));
}

#if 0
SCM_DEFINE (G_get_vram_info, "get-vram-info", 1, 0, 0, (SCM index),"\
Return a list that describes the state of a VRAM bank.")
{
    return scm_list_5( scm_cons(scm_from_utf8_symbol ("name"),
                                scm_from_utf8_string (vram_bank_name[i])),
                       scm_cons(scm_from_utf8_symbol ("max-width"),
                                scm_from_int (vram_width[i])),
                       scm_cons(scm_from_utf8_symbol ("max-height"),
                                scm_from_int (vram_height[i])),
                       scm_cons(scm_from_utf8_symbol ("type")));
}
#endif                                


SCM_DEFINE (G_load_image_file, "load-image-file",
            2, 0, 0, (SCM filename, SCM index), "\
This procedure loads an image file and stores it into a VRAM bank. If\n\
it doesn't fit in the VRAM bank, it just stores as much will fit,\n\
beginning from the top-left corner.  The image file needs to contain\n\
32-bit RGB color.")
{
    SCM_ASSERT (scm_is_string (filename), filename, SCM_ARG1, "load-image-file");
    
    char *path = scm_to_locale_string (filename);
    gboolean ret = 
        set_vram_to_pixbuf_from_image_file (scm_to_int(index), path);
    free(path);
    vram_info_list_store_update(vram_list_store);
    return scm_from_bool(ret);
}

SCM_DEFINE (G_load_audio_file, "load-audio-file", 2, 0, 0,
            (SCM filename, SCM index), "\
The procedure loads an audio file and stores it, unmodified,\n\
into a VRAM bank.  If it doesn't fit, it ddoes nothing. Check the list\n\
of supported formats.")
{
    SCM_ASSERT (scm_is_string (filename), filename, SCM_ARG1, "load-audio-file");
    int vram = scm_to_int(index);
    char *path = scm_to_locale_string (filename);

    if (!canvas_vram_is_valid_filename (path))
    {
        g_message ("load-audio-file: filename %s is invalid", path);
        free (path);
        return SCM_BOOL_F;
    }
    
    if (!g_str_has_suffix (path, ".ogg")
        && g_str_has_suffix (path, ".ogv"))
    {
        g_message ("load-audio-file: %s doesn't have an .ogg or .ogv extension\n", path);
        free (path);
        return SCM_BOOL_F;
    }

    char *pathname;
    if (vram_path)
        pathname = g_build_filename (vram_path, path, NULL);
    else
        pathname = strdup(path);
    
    GFile *f = g_file_new_for_path (pathname);
    g_free (pathname);
    GError *error = NULL;
    GFileInfo *fi = g_file_query_info (f, 
                                       "standard::size,standard::display-name",
                                       0,
                                       NULL,
                                       &error);
    if (fi == NULL)
    {
        g_message ("load-audio-file: %s", error->message);
        g_error_free (error);
    }
    else if (g_file_info_get_size (fi) > 4 * vram_size[vram])
    {
        g_message ("load-audio-file: file %s is too large for %s",
                   g_file_info_get_display_name (fi),
                   vram_bank_name[vram]);
    }
    else 
    {
        GFileInputStream *in = g_file_read (f, NULL, &error);
        if (in == NULL)
        {
            g_message ("load-audio-file: %s", error->message);
            g_error_free (error);
        }
        else
        {
            gsize bytes_read;
            gboolean ret = g_input_stream_read_all (G_INPUT_STREAM (in),
                                                    vram_ptr[vram],
                                                    4 * vram_size[vram],
                                                    &bytes_read,
                                                    NULL,
                                                    &error);
            if (ret)
            {
                vram_info[vram].type = VRAM_TYPE_AUDIO;
                free (vram_info[vram].filename);
                vram_info[vram].filename = g_strdup(g_file_info_get_display_name (fi));
                vram_info[vram].size = bytes_read;
                g_info ("Loaded audio file %s into %s.",
                        vram_info[vram].filename,
                        vram_bank_name[vram]);
                vram_info_list_store_update(vram_list_store);
            }
            else
            {
                g_message ("load-audio-file: %s", error->message);
                g_error_free (error);
            }
            g_input_stream_close (G_INPUT_STREAM (in), NULL, NULL);
            g_object_unref (in);
        }
    }
    g_object_unref (fi);
    g_object_unref (f);
    free (path);
    return SCM_UNSPECIFIED;
}

void
canvas_vram_init_guile_procedures (void)
{
#ifndef SCM_MAGIC_SNARFER
#include "canvas_vram.x"
#endif
    scm_c_export (
        "VRAM_A",
        "VRAM_B",
        "VRAM_C",
        "VRAM_D",
        "VRAM_E",
        "VRAM_F",
        "VRAM_G",
        "VRAM_H",
        "VRAM_I",
        "VRAM_J",
        "get-vram-type",
        "get-vram-filename",
        "get-vram-image-size",
        "VRAM_INDEX_LIST",
        "vram-get-u32-size",
        "load-image-file",
        "load-audio-file",
        NULL);
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

