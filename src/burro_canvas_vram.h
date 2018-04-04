#ifndef BURRO_CANVAS_VRAM_H
#define BURRO_CANVAS_VRAM_H

#include <libguile.h>
#include <gtk/gtk.h>

/* VRAM
 *
 * To avoid overloading memory, Burro Engine has strictly limited
 * space to store graphics and audio.  This system is called VRAM for
 * historical and perverse reasons.
 *
 * When complete, there are 9 VRAM banks which can be used for
 * - 32bpp pixmaps
 * - Compressed audio
 *
 * There are four large banks, one medium bank, and 1 small bank.
 *
 *  BANK      RAW    GRAPHICS         AUDIO     
 *  VRAM_A,   1MB,   512x512x4        ~8 minutes
 *  VRAM_B,
 *  VRAM_C,
 *  VRAM_D,
 *  VRAM_E,   64kB   128x128x4        ~8 seconds
 *  VRAM_F,   16kB   64x64x4                    
 *  VRAM_G,
 *  VRAM_H,
 *  VRAM_I,
 *  VRAM_J
 *
 */

enum {
    VRAM_TYPE_RAW,
    VRAM_TYPE_IMAGE,		/* 32-bit image data */
    VRAM_TYPE_AUDIO,		/* Complete audio file */
    VRAM_N_TYPES
};

typedef struct vram_info_tag {
    int type;
    int width;			/* for IMAGE */
    int height;			/* for IMAGE */
    int size;			/* for AUDIO, length in bytes */
    char *filename;		/* the source of the contents */
} vram_info_t;

/** Index of a VRAM bank. */
typedef enum {
    VRAM_NONE,
    VRAM_A,                     /**< 256k uint32 */
    VRAM_B,                     /**< 256k uint32 */
    VRAM_C,                     /**< 256k uint32  */
    VRAM_D,                     /**< 256k uint32  */
    VRAM_E,                     /**< 64k uint32 */
    VRAM_F,                     /**< 4k uint32  */
    VRAM_G,                     /**< 4k uint32  */
    VRAM_H,                     /**< 4k uint32  */
    VRAM_I,                     /**< 4k uint32  */
    VRAM_J,                     /**< 4k uint32  */
    VRAM_COUNT,
} vram_bank_t;

////////////////////////////////////////////////////////////////
//

#define VRAM_NONE_U32_HEIGHT (0)
#define VRAM_NONE_U32_WIDTH (0)
#define VRAM_NONE_U32_SIZE (0)
#define VRAM_NONE_U32_OFFSET (0)
#define VRAM_NONE_U32_PTR (0)

////////////////////////////////////////////////////////////////
//
// VRAM ABCD: 4MB of storage

#define VRAM_A_U32_HEIGHT (1024)
#define VRAM_A_U32_WIDTH (1024)
#define VRAM_B_U32_HEIGHT (1024)
#define VRAM_B_U32_WIDTH (1024)
#define VRAM_C_U32_HEIGHT (512)
#define VRAM_C_U32_WIDTH (512)
#define VRAM_D_U32_HEIGHT (512)
#define VRAM_D_U32_WIDTH (512)

#define VRAM_A_U32_SIZE (VRAM_A_U32_HEIGHT*VRAM_A_U32_WIDTH)
#define VRAM_B_U32_SIZE (VRAM_B_U32_HEIGHT*VRAM_B_U32_WIDTH)
#define VRAM_C_U32_SIZE (VRAM_C_U32_HEIGHT*VRAM_C_U32_WIDTH)
#define VRAM_D_U32_SIZE (VRAM_D_U32_HEIGHT*VRAM_D_U32_WIDTH)
#define VRAM_ABCD_U32_SIZE \
  (VRAM_A_U32_SIZE + VRAM_B_U32_SIZE + VRAM_C_U32_SIZE + VRAM_D_U32_SIZE)

#define VRAM_A_U32_OFFSET (0)
#define VRAM_B_U32_OFFSET (VRAM_A_U32_OFFSET + VRAM_A_U32_SIZE)
#define VRAM_C_U32_OFFSET (VRAM_B_U32_OFFSET + VRAM_B_U32_SIZE)
#define VRAM_D_U32_OFFSET (VRAM_C_U32_OFFSET + VRAM_C_U32_SIZE)

extern  uint32_t vram_ABCD_store[VRAM_ABCD_U32_SIZE];

#define VRAM_A_U32_PTR    (vram_ABCD_store + VRAM_A_U32_OFFSET)
#define VRAM_B_U32_PTR    (vram_ABCD_store + VRAM_B_U32_OFFSET)
#define VRAM_C_U32_PTR    (vram_ABCD_store + VRAM_C_U32_OFFSET)
#define VRAM_D_U32_PTR    (vram_ABCD_store + VRAM_D_U32_OFFSET)

////////////////////////////////////////////////////////////////
//
// VRAM EFGHI

#define VRAM_E_U32_HEIGHT (256)
#define VRAM_E_U32_WIDTH (256)
#define VRAM_F_U32_HEIGHT (256)
#define VRAM_F_U32_WIDTH (256)
#define VRAM_G_U32_HEIGHT (128)
#define VRAM_G_U32_WIDTH (128)
#define VRAM_H_U32_HEIGHT (128)
#define VRAM_H_U32_WIDTH (128)
#define VRAM_I_U32_HEIGHT (64)
#define VRAM_I_U32_WIDTH (64)
#define VRAM_J_U32_HEIGHT (64)
#define VRAM_J_U32_WIDTH (64)

#define VRAM_E_U32_SIZE (VRAM_E_U32_HEIGHT*VRAM_E_U32_WIDTH)
#define VRAM_F_U32_SIZE (VRAM_F_U32_HEIGHT*VRAM_F_U32_WIDTH)
#define VRAM_G_U32_SIZE (VRAM_G_U32_HEIGHT*VRAM_G_U32_WIDTH)
#define VRAM_H_U32_SIZE (VRAM_H_U32_HEIGHT*VRAM_H_U32_WIDTH)
#define VRAM_I_U32_SIZE (VRAM_I_U32_HEIGHT*VRAM_I_U32_WIDTH)
#define VRAM_J_U32_SIZE (VRAM_J_U32_HEIGHT*VRAM_J_U32_WIDTH)
#define VRAM_EFGHIJ_U32_SIZE \
    (VRAM_E_U32_SIZE + VRAM_F_U32_SIZE + VRAM_G_U32_SIZE + VRAM_H_U32_SIZE \
     + VRAM_I_U32_SIZE + VRAM_J_U32_SIZE)

#define VRAM_E_U32_OFFSET (0)
#define VRAM_F_U32_OFFSET (VRAM_E_U32_OFFSET + VRAM_E_U32_SIZE)
#define VRAM_G_U32_OFFSET (VRAM_F_U32_OFFSET + VRAM_F_U32_SIZE)
#define VRAM_H_U32_OFFSET (VRAM_G_U32_OFFSET + VRAM_G_U32_SIZE)
#define VRAM_I_U32_OFFSET (VRAM_H_U32_OFFSET + VRAM_H_U32_SIZE)
#define VRAM_J_U32_OFFSET (VRAM_I_U32_OFFSET + VRAM_I_U32_SIZE)

extern uint32_t vram_EFGHIJ_store[VRAM_EFGHIJ_U32_SIZE];

#define VRAM_E_U32_PTR (vram_EFGHIJ_store + VRAM_E_U32_OFFSET)
#define VRAM_F_U32_PTR (vram_EFGHIJ_store + VRAM_F_U32_OFFSET)
#define VRAM_G_U32_PTR (vram_EFGHIJ_store + VRAM_G_U32_OFFSET)
#define VRAM_H_U32_PTR (vram_EFGHIJ_store + VRAM_H_U32_OFFSET)
#define VRAM_I_U32_PTR (vram_EFGHIJ_store + VRAM_I_U32_OFFSET)
#define VRAM_J_U32_PTR (vram_EFGHIJ_store + VRAM_J_U32_OFFSET)

////////////////////////////////////////////////////////////////

gboolean vram_validate_int_as_vram_bank_t (int x);

void vram_init (void);

const char *
vram_get_bank_name (vram_bank_t bank);

/** Return the size, in 32-bit words, of a VRAM bank.
 *  @param [in] bank
 *  @return size of bank of VRAM in 32-bit words
 */
int vram_get_u32_size (vram_bank_t bank);

int vram_get_u32_height (vram_bank_t bank);
int vram_get_u32_width (vram_bank_t bank);

/** Return a pointer to the beginning of the VRAM bank.
 *  @param [in] bank index
 *  @return a pointer to the VRAM bank
 */
uint32_t *vram_get_u32_ptr (vram_bank_t bank);

/** Zero-fill the contents of a VRAM bank.
 *  @param [in] bank index
 */
void vram_zero_bank (vram_bank_t bank);

////////////////////////////////////////////////////////////////
typedef struct vram_io_context_tag {
    int index;
    gboolean open;
    void *ptr;
    long size;
    long position;
} vram_io_context_t;


vram_io_context_t *vram_audio_open (int index);
size_t vram_audio_read (void *ptr, size_t size, size_t nmemb, void *context);
int vram_audio_seek (void *context, gint64 offset, int whence);
int vram_audio_close (void *context);
long vram_audio_tell (void *context);


////////////////////////////////////////////////////////////////
SCM _scm_from_vram_bank_t (vram_bank_t x);
vram_bank_t _scm_to_vram_bank_t (SCM x);
gboolean _scm_is_vram_bank_t (SCM x);

/** Register VRAM procedures with the script engine. */
void burro_canvas_vram_init_guile_procedures (void);

enum {
    VRAM_COLUMN_NAME,                /* The name of the bank, like "VRAM A" */
    VRAM_COLUMN_TYPE,
    VRAM_COLUMN_FILENAME,
    VRAM_COLUMN_SIZE,
    VRAM_N_COLUMNS
};

GtkListStore *vram_info_list_store_new();
void vram_info_list_store_update(GtkListStore *list_store);

int vram_get_type (int z);
int vram_get_width (int z);
int vram_get_height (int z);

#endif

/*
  Local Variables:
  mode:C
  c-file-style:"linux"
  tab-width:4
  c-basic-offset: 4
  indent-tabs-mode:nil
  End:
*/
