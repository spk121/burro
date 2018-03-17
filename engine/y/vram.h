/** @file vram.h
    A set of pre-allocated, memory-aligned buffers used for storage
    of BG maps, BG bitmaps, BG tilesheets, and OBJ spritesheets.
*/

#ifndef BURRO_VRAM_H
#define BURRO_VRAM_H

#include <stdalign.h>
#include <stdint.h>

/** Index of a VRAM bank. */
typedef enum {
    VRAM_A,                     /**< 64k uint32, usually for main spritesheet  */
    VRAM_B,                     /**< 64k uint32, usually for sub spritesheet  */
    VRAM_C,                     /**< 64k uint32  */
    VRAM_D,                     /**< 64k uint32  */
    VRAM_E,                     /**< 16k uint32, usually for bg maps  */
    VRAM_F,                     /**< 1k uint32, usually for bg maps  */
    VRAM_G,                     /**< 1k uint32, usually for bg maps  */
    VRAM_H,                     /**< 1k uint32, usually for bg maps  */
    VRAM_I,                     /**< 1k uint32, usually for bg maps  */
    VRAM_J,                     /**< 1k uint32, usually for bg maps  */
    VRAM_AB,                    /**< 128k uint32, use A and B as a
                                 * single block, main screen only  */
    VRAM_CD,                    /**< 128k uint32, use C and D as a
                                 * single block, main screen only  */
    VRAM_ABCD,                  /**< 256k uint32, use A, B, C, and D
                                 * as single block, main screen only  */
    VRAM_COUNT,
} vram_bank_t;

////////////////////////////////////////////////////////////////
//

////////////////////////////////////////////////////////////////
//
// VRAM ABCD: 1MB of storage
// Best used for BG BMP and for OBJ SPRITESHEETS
// Can be used as 4 smaller chunks, 2 medium chunks, or 1 larger chunk.
// Best used as 256x256px BG BMP or OBJ Spritesheets

#define VRAM_A_U32_SIZE (256*256)
#define VRAM_B_U32_SIZE (256*256)
#define VRAM_C_U32_SIZE (256*256)
#define VRAM_D_U32_SIZE (256*256)
#define VRAM_AB_U32_SIZE (VRAM_A_U32_SIZE + VRAM_B_U32_SIZE)
#define VRAM_CD_U32_SIZE (VRAM_C_U32_SIZE + VRAM_D_U32_SIZE)
#define VRAM_ABCD_U32_SIZE (VRAM_AB_U32_SIZE + VRAM_CD_U32_SIZE)

#define VRAM_A_U32_OFFSET (0)
#define VRAM_B_U32_OFFSET (VRAM_A_U32_OFFSET + VRAM_A_U32_SIZE)
#define VRAM_C_U32_OFFSET (VRAM_B_U32_OFFSET + VRAM_B_U32_SIZE)
#define VRAM_D_U32_OFFSET (VRAM_C_U32_OFFSET + VRAM_C_U32_SIZE)

#define VRAM_ABCD_U32_OFFSET (VRAM_A_U32_OFFSET)
#define VRAM_AB_U32_OFFSET (VRAM_A_U32_OFFSET)
#define VRAM_CD_U32_OFFSET (VRAM_C_U32_OFFSET)

extern alignas(16) uint32_t vram_ABCD_store[VRAM_ABCD_U32_SIZE];

#define VRAM_A_U32_PTR    (vram_ABCD_store + VRAM_A_U32_OFFSET)
#define VRAM_B_U32_PTR    (vram_ABCD_store + VRAM_B_U32_OFFSET)
#define VRAM_C_U32_PTR    (vram_ABCD_store + VRAM_C_U32_OFFSET)
#define VRAM_D_U32_PTR    (vram_ABCD_store + VRAM_D_U32_OFFSET)
#define VRAM_AB_U32_PTR   (vram_ABCD_store + VRAM_AB_U32_OFFSET)
#define VRAM_CD_U32_PTR   (vram_ABCD_store + VRAM_CD_U32_OFFSET)
#define VRAM_ABCD_U32_PTR (vram_ABCD_store + VRAM_ABCD_U32_OFFSET)

////////////////////////////////////////////////////////////////
//
// VRAM EFGHI: 80kb of storage
// Best used for BG MAP storage

#define VRAM_E_U32_SIZE (128*128)
#define VRAM_F_U32_SIZE (32*32)
#define VRAM_G_U32_SIZE (32*32)
#define VRAM_H_U32_SIZE (32*32)
#define VRAM_I_U32_SIZE (32*32)
#define VRAM_J_U32_SIZE (32*32)
#define VRAM_EFGHIJ_U32_SIZE \
    (VRAM_E_U32_SIZE + VRAM_F_U32_SIZE + VRAM_G_U32_SIZE + VRAM_H_U32_SIZE \
     + VRAM_I_U32_SIZE + VRAM_J_U32_SIZE)

#define VRAM_E_U32_OFFSET (0)
#define VRAM_F_U32_OFFSET (VRAM_E_U32_OFFSET + VRAM_E_U32_SIZE)
#define VRAM_G_U32_OFFSET (VRAM_F_U32_OFFSET + VRAM_F_U32_SIZE)
#define VRAM_H_U32_OFFSET (VRAM_G_U32_OFFSET + VRAM_G_U32_SIZE)
#define VRAM_I_U32_OFFSET (VRAM_H_U32_OFFSET + VRAM_H_U32_SIZE)
#define VRAM_J_U32_OFFSET (VRAM_I_U32_OFFSET + VRAM_I_U32_SIZE)

extern alignas(16) uint32_t vram_EFGHIJ_store[VRAM_EFGHIJ_U32_SIZE];

#define VRAM_E_U32_PTR (vram_EFGHIJ_store + VRAM_E_U32_OFFSET)
#define VRAM_F_U32_PTR (vram_EFGHIJ_store + VRAM_F_U32_OFFSET)
#define VRAM_G_U32_PTR (vram_EFGHIJ_store + VRAM_G_U32_OFFSET)
#define VRAM_H_U32_PTR (vram_EFGHIJ_store + VRAM_H_U32_OFFSET)
#define VRAM_I_U32_PTR (vram_EFGHIJ_store + VRAM_I_U32_OFFSET)
#define VRAM_J_U32_PTR (vram_EFGHIJ_store + VRAM_J_U32_OFFSET)

////////////////////////////////////////////////////////////////

bool vram_validate_int_as_vram_bank_t (int x);

void vram_init (void);

const char *
vram_get_bank_name (vram_bank_t bank);

/** Return the size, in 32-bit words, of a VRAM bank.
 *  @param [in] bank
 *  @return size of bank of VRAM in 32-bit words
 */
int vram_get_u32_size (vram_bank_t bank);

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
SCM _scm_from_vram_bank_t (vram_bank_t x);
vram_bank_t _scm_to_vram_bank_t (SCM x);
bool _scm_is_vram_bank_t (SCM x);

/** Register VRAM procedures with the script engine. */
void vram_init_guile_procedures (void);


#endif
