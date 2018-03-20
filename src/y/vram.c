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

#include <stdalign.h>
#include "../x.h"
#include "vram.h"

alignas(16) uint32_t vram_ABCD_store[VRAM_ABCD_U32_SIZE];
alignas(16) uint32_t vram_EFGHIJ_store[VRAM_EFGHIJ_U32_SIZE];

static int vram_size[VRAM_COUNT] = {
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
    [VRAM_AB] = VRAM_AB_U32_SIZE,
    [VRAM_CD] = VRAM_CD_U32_SIZE,
    [VRAM_ABCD] = VRAM_ABCD_U32_SIZE
};

static uint32_t *vram_ptr[VRAM_COUNT] = {
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
    [VRAM_AB] = VRAM_AB_U32_PTR,
    [VRAM_CD] = VRAM_D_U32_PTR,
    [VRAM_ABCD] = VRAM_ABCD_U32_PTR,
};

static const char 
vram_bank_name[VRAM_COUNT][10] = {
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
    [VRAM_AB] = "VRAM_AB",
    [VRAM_CD] = "VRAM_CD",
    [VRAM_ABCD] = "VRAM_ABCD",
};

static bool
vram_validate_vram_bank_t (vram_bank_t x)
{
    return (x >= VRAM_A && x <= VRAM_ABCD);
}


bool
vram_validate_int_as_vram_bank_t (int x)
{
    return (x >= (int) VRAM_A && x <= (int) VRAM_ABCD);
}

void
vram_init (void)
{
    vram_zero_bank(VRAM_ABCD);
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

////////////////////////////////////////////////////////////////
SCM _scm_from_vram_bank_t (vram_bank_t x)
{
    return scm_from_int ((int) x);
}

vram_bank_t _scm_to_vram_bank_t (SCM x)
{
    return (vram_bank_t) scm_to_int (x);
}

bool _scm_is_vram_bank_t (SCM x)
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
SCM_VARIABLE_INIT (G_VRAM_AB, "VRAM_AB", _scm_from_vram_bank_t (VRAM_AB));
SCM_VARIABLE_INIT (G_VRAM_CD, "VRAM_CD", _scm_from_vram_bank_t (VRAM_CD));
SCM_VARIABLE_INIT (G_VRAM_ABCD, "VRAM_ABCD", _scm_from_vram_bank_t (VRAM_ABCD));
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
                              G_VRAM_AB,
                              G_VRAM_CD,
                              G_VRAM_ABCD,
                              SCM_UNDEFINED));

#define SCM_BV(_bank) \
    scm_pointer_to_bytevector(scm_from_pointer(VRAM_ ## _bank ## _U32_PTR, NULL), scm_from_int(VRAM_ ## _bank ## _U32_SIZE), scm_from_int (0), scm_from_locale_symbol("u32"))

SCM_VARIABLE_INIT (G_vram_A_bv, "vram-A-bv", SCM_BV(A));
SCM_VARIABLE_INIT (G_vram_B_bv, "vram-B-bv", SCM_BV(B));
SCM_VARIABLE_INIT (G_vram_C_bv, "vram-C-bv", SCM_BV(C));
SCM_VARIABLE_INIT (G_vram_D_bv, "vram-D-bv", SCM_BV(D));
SCM_VARIABLE_INIT (G_vram_E_bv, "vram-E-bv", SCM_BV(E));
SCM_VARIABLE_INIT (G_vram_F_bv, "vram-F-bv", SCM_BV(F));
SCM_VARIABLE_INIT (G_vram_G_bv, "vram-G-bv", SCM_BV(G));
SCM_VARIABLE_INIT (G_vram_H_bv, "vram-H-bv", SCM_BV(H));
SCM_VARIABLE_INIT (G_vram_I_bv, "vram-I-bv", SCM_BV(I));
SCM_VARIABLE_INIT (G_vram_J_bv, "vram-J-bv", SCM_BV(J));
SCM_VARIABLE_INIT (G_vram_AB_bv, "vram-AB-bv", SCM_BV(AB));
SCM_VARIABLE_INIT (G_vram_CD_bv, "vram-CD-bv", SCM_BV(CD));
SCM_VARIABLE_INIT (G_vram_ABCD_bv, "vram-ABCD-bv", SCM_BV(ABCD));
SCM_VARIABLE_INIT (G_vram_bv_list, "vram-bv-list",
                   scm_list_n(G_vram_A_bv,
                              G_vram_B_bv,
                              G_vram_C_bv,
                              G_vram_D_bv,
                              G_vram_E_bv,
                              G_vram_F_bv,
                              G_vram_G_bv,
                              G_vram_H_bv,
                              G_vram_I_bv,
                              G_vram_J_bv,
                              G_vram_AB_bv,
                              G_vram_CD_bv,
                              G_vram_ABCD_bv,
                              SCM_UNDEFINED));

SCM_DEFINE (G_vram_get_u32_size, "vram-get-u32-size", 1, 0, 0, (SCM index),
    "Given an index that represents a VRAM bank, return the maximum number of \n\
32-bit integers that it could contain in its bytevector.")
{
    SCM_ASSERT(_scm_is_vram_bank_t(index), index, SCM_ARG1, "vram-get-u32-size");
    
    return scm_from_int (vram_get_u32_size (_scm_to_vram_bank_t (index)));
}

void
vram_init_guile_procedures (void)
{
#include "vram.x"
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
                  "VRAM_AB",
                  "VRAM_CD",
                  "VRAM_ABCD",
                  "VRAM_INDEX_LIST",
                  "vram-A-bv",
                  "vram-B-bv",
                  "vram-C-bv",
                  "vram-D-bv",
                  "vram-E-bv",
                  "vram-F-bv",
                  "vram-G-bv",
                  "vram-H-bv",
                  "vram-I-bv",
                  "vram-J-bv",
                  "vram-AB-bv",
                  "vram-CD-bv",
                  "vram-ABCD-bv",
                  "vram-bv-list",
                  "vram-get-u32-size",
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

