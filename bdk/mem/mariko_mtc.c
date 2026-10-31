/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stddef.h>
#include <soc/timer.h>
#include <soc/t210.h>
#include <soc/fuse.h>
#include <gfx_utils.h>

#include "mtc_timing_table_mariko.h"
#include "mtc_uncompress.h"
#include "reg.h"
#include "mtc_div_up.h"

static const uintptr_t CLKRST = 0x60006000;
static const uintptr_t MC     = MC_BASE;
static const uintptr_t EMC    = EMC_BASE;
static const uintptr_t EMC0   = EMC0_BASE;
static const uintptr_t EMC1   = EMC1_BASE;

static bool g_next_pll           = false;
static bool g_did_first_training = false;
static bool g_fsp_for_next_freq  = false;
u8 mtc_tables_buffer[0x26C0];

/* Timing in use once trained, so periodic training can reuse its state. */
static EmcDvfsTimingTable *g_periodic_timing = NULL;

extern char __end__;
char __end__ __attribute__((weak)) = 0;

#include "mtc_tables_mariko.inc"

/* Biggest hack ever */
typedef struct {
    uint32_t dq[0x100];
    uint8_t  dmi[0x100];
} EmcRamTrainingPattern;

const u8 EmcRamTrainingPatternData[] = {
    0x18, 0x18, 0x18, 0x18, 0x61, 0x61, 0x61, 0x61, 0x85, 0x85, 0x85, 0x85, 0x14, 0x14, 0x14, 0x14,
    0x51, 0x51, 0x51, 0x51, 0x47, 0x47, 0x47, 0x47, 0x1E, 0x1E, 0x1E, 0x1E, 0x79, 0x79, 0x79, 0x79,
    0xE5, 0xE5, 0xE5, 0xE5, 0x94, 0x94, 0x94, 0x94, 0x51, 0x51, 0x51, 0x51, 0x46, 0x46, 0x46, 0x46,
    0x19, 0x19, 0x19, 0x19, 0x67, 0x67, 0x67, 0x67, 0x9C, 0x9C, 0x9C, 0x9C, 0x71, 0x71, 0x71, 0x71,
    0xC5, 0xC5, 0xC5, 0xC5, 0x17, 0x17, 0x17, 0x17, 0x5F, 0x5F, 0x5F, 0x5F, 0x7E, 0x7E, 0x7E, 0x7E,
    0xFB, 0xFB, 0xFB, 0xFB, 0xED, 0xED, 0xED, 0xED, 0xB4, 0xB4, 0xB4, 0xB4, 0xD2, 0xD2, 0xD2, 0xD2,
    0x48, 0x48, 0x48, 0x48, 0x21, 0x21, 0x21, 0x21, 0x85, 0x85, 0x85, 0x85, 0x16, 0x16, 0x16, 0x16,
    0x59, 0x59, 0x59, 0x59, 0x66, 0x66, 0x66, 0x66, 0x9A, 0x9A, 0x9A, 0x9A, 0x69, 0x69, 0x69, 0x69,
    0xA4, 0xA4, 0xA4, 0xA4, 0x93, 0x93, 0x93, 0x93, 0x4F, 0x4F, 0x4F, 0x4F, 0x3F, 0x3F, 0x3F, 0x3F,
    0xFC, 0xFC, 0xFC, 0xFC, 0xF3, 0xF3, 0xF3, 0xF3, 0xCD, 0xCD, 0xCD, 0xCD, 0x37, 0x37, 0x37, 0x37,
    0xDC, 0xDC, 0xDC, 0xDC, 0x70, 0x70, 0x70, 0x70, 0xC3, 0xC3, 0xC3, 0xC3, 0x0F, 0x0F, 0x0F, 0x0F,
    0x3E, 0x3E, 0x3E, 0x3E, 0xFA, 0xFA, 0xFA, 0xFA, 0xEB, 0xEB, 0xEB, 0xEB, 0xAC, 0xAC, 0xAC, 0xAC,
    0xB3, 0xB3, 0xB3, 0xB3, 0xCC, 0xCC, 0xCC, 0xCC, 0x31, 0x31, 0x31, 0x31, 0xC5, 0xC5, 0xC5, 0xC5,
    0x15, 0x15, 0x15, 0x15, 0x57, 0x57, 0x57, 0x57, 0x5F, 0x5F, 0x5F, 0x5F, 0x7F, 0x7F, 0x7F, 0x7F,
    0xFD, 0xFD, 0xFD, 0xFD, 0xF4, 0xF4, 0xF4, 0xF4, 0xD0, 0xD0, 0xD0, 0xD0, 0x42, 0x42, 0x42, 0x42,
    0x08, 0x08, 0x08, 0x08, 0x23, 0x23, 0x23, 0x23, 0x8F, 0x8F, 0x8F, 0x8F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x18, 0x18, 0x18, 0x18, 0x61, 0x61, 0x61, 0x61, 0x85, 0x85, 0x85, 0x85, 0x14, 0x14, 0x14, 0x14,
    0x51, 0x51, 0x51, 0x51, 0x47, 0x47, 0x47, 0x47, 0x1E, 0x1E, 0x1E, 0x1E, 0x79, 0x79, 0x79, 0x79,
    0xE5, 0xE5, 0xE5, 0xE5, 0x94, 0x94, 0x94, 0x94, 0x51, 0x51, 0x51, 0x51, 0x46, 0x46, 0x46, 0x46,
    0x19, 0x19, 0x19, 0x19, 0x67, 0x67, 0x67, 0x67, 0x9C, 0x9C, 0x9C, 0x9C, 0x71, 0x71, 0x71, 0x71,
    0xC5, 0xC5, 0xC5, 0xC5, 0x17, 0x17, 0x17, 0x17, 0x5F, 0x5F, 0x5F, 0x5F, 0x7E, 0x7E, 0x7E, 0x7E,
    0xFB, 0xFB, 0xFB, 0xFB, 0xED, 0xED, 0xED, 0xED, 0xB4, 0xB4, 0xB4, 0xB4, 0xD2, 0xD2, 0xD2, 0xD2,
    0x48, 0x48, 0x48, 0x48, 0x21, 0x21, 0x21, 0x21, 0x85, 0x85, 0x85, 0x85, 0x16, 0x16, 0x16, 0x16,
    0x59, 0x59, 0x59, 0x59, 0x66, 0x66, 0x66, 0x66, 0x9A, 0x9A, 0x9A, 0x9A, 0x69, 0x69, 0x69, 0x69,
    0xA4, 0xA4, 0xA4, 0xA4, 0x93, 0x93, 0x93, 0x93, 0x4F, 0x4F, 0x4F, 0x4F, 0x3F, 0x3F, 0x3F, 0x3F,
    0xFC, 0xFC, 0xFC, 0xFC, 0xF3, 0xF3, 0xF3, 0xF3, 0xCD, 0xCD, 0xCD, 0xCD, 0x37, 0x37, 0x37, 0x37,
    0xDC, 0xDC, 0xDC, 0xDC, 0x70, 0x70, 0x70, 0x70, 0xC3, 0xC3, 0xC3, 0xC3, 0x0F, 0x0F, 0x0F, 0x0F,
    0x3E, 0x3E, 0x3E, 0x3E, 0xFA, 0xFA, 0xFA, 0xFA, 0xEB, 0xEB, 0xEB, 0xEB, 0xAC, 0xAC, 0xAC, 0xAC,
    0xB3, 0xB3, 0xB3, 0xB3, 0xCC, 0xCC, 0xCC, 0xCC, 0x31, 0x31, 0x31, 0x31, 0xC5, 0xC5, 0xC5, 0xC5,
    0x15, 0x15, 0x15, 0x15, 0x57, 0x57, 0x57, 0x57, 0x5F, 0x5F, 0x5F, 0x5F, 0x7F, 0x7F, 0x7F, 0x7F,
    0xFD, 0xFD, 0xFD, 0xFD, 0xF4, 0xF4, 0xF4, 0xF4, 0xD0, 0xD0, 0xD0, 0xD0, 0x42, 0x42, 0x42, 0x42,
    0x08, 0x08, 0x08, 0x08, 0x23, 0x23, 0x23, 0x23, 0x8F, 0x8F, 0x8F, 0x8F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x06, 0x06, 0x06, 0x06, 0x18, 0x18, 0x18, 0x18, 0x21, 0x21, 0x21, 0x21, 0x05, 0x05, 0x05, 0x05,
    0x14, 0x14, 0x14, 0x14, 0x11, 0x11, 0x11, 0x11, 0x07, 0x07, 0x07, 0x07, 0x1E, 0x1E, 0x1E, 0x1E,
    0x39, 0x39, 0x39, 0x39, 0x25, 0x25, 0x25, 0x25, 0x14, 0x14, 0x14, 0x14, 0x11, 0x11, 0x11, 0x11,
    0x06, 0x06, 0x06, 0x06, 0x19, 0x19, 0x19, 0x19, 0x27, 0x27, 0x27, 0x27, 0x1C, 0x1C, 0x1C, 0x1C,
    0x31, 0x31, 0x31, 0x31, 0x05, 0x05, 0x05, 0x05, 0x17, 0x17, 0x17, 0x17, 0x1F, 0x1F, 0x1F, 0x1F,
    0x3E, 0x3E, 0x3E, 0x3E, 0x3B, 0x3B, 0x3B, 0x3B, 0x2D, 0x2D, 0x2D, 0x2D, 0x34, 0x34, 0x34, 0x34,
    0x12, 0x12, 0x12, 0x12, 0x08, 0x08, 0x08, 0x08, 0x21, 0x21, 0x21, 0x21, 0x05, 0x05, 0x05, 0x05,
    0x16, 0x16, 0x16, 0x16, 0x19, 0x19, 0x19, 0x19, 0x26, 0x26, 0x26, 0x26, 0x1A, 0x1A, 0x1A, 0x1A,
    0x29, 0x29, 0x29, 0x29, 0x24, 0x24, 0x24, 0x24, 0x13, 0x13, 0x13, 0x13, 0x0F, 0x0F, 0x0F, 0x0F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3C, 0x3C, 0x3C, 0x3C, 0x33, 0x33, 0x33, 0x33, 0x0D, 0x0D, 0x0D, 0x0D,
    0x37, 0x37, 0x37, 0x37, 0x1C, 0x1C, 0x1C, 0x1C, 0x30, 0x30, 0x30, 0x30, 0x03, 0x03, 0x03, 0x03,
    0x0F, 0x0F, 0x0F, 0x0F, 0x3E, 0x3E, 0x3E, 0x3E, 0x3A, 0x3A, 0x3A, 0x3A, 0x2B, 0x2B, 0x2B, 0x2B,
    0x2C, 0x2C, 0x2C, 0x2C, 0x33, 0x33, 0x33, 0x33, 0x0C, 0x0C, 0x0C, 0x0C, 0x31, 0x31, 0x31, 0x31,
    0x05, 0x05, 0x05, 0x05, 0x15, 0x15, 0x15, 0x15, 0x17, 0x17, 0x17, 0x17, 0x1F, 0x1F, 0x1F, 0x1F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3D, 0x3D, 0x3D, 0x3D, 0x34, 0x34, 0x34, 0x34, 0x10, 0x10, 0x10, 0x10,
    0x02, 0x02, 0x02, 0x02, 0x08, 0x08, 0x08, 0x08, 0x23, 0x23, 0x23, 0x23, 0x0F, 0x0F, 0x0F, 0x0F,
    0x06, 0x06, 0x06, 0x06, 0x18, 0x18, 0x18, 0x18, 0x21, 0x21, 0x21, 0x21, 0x05, 0x05, 0x05, 0x05,
    0x14, 0x14, 0x14, 0x14, 0x11, 0x11, 0x11, 0x11, 0x07, 0x07, 0x07, 0x07, 0x1E, 0x1E, 0x1E, 0x1E,
    0x39, 0x39, 0x39, 0x39, 0x25, 0x25, 0x25, 0x25, 0x14, 0x14, 0x14, 0x14, 0x11, 0x11, 0x11, 0x11,
    0x06, 0x06, 0x06, 0x06, 0x19, 0x19, 0x19, 0x19, 0x27, 0x27, 0x27, 0x27, 0x1C, 0x1C, 0x1C, 0x1C,
    0x31, 0x31, 0x31, 0x31, 0x05, 0x05, 0x05, 0x05, 0x17, 0x17, 0x17, 0x17, 0x1F, 0x1F, 0x1F, 0x1F,
    0x3E, 0x3E, 0x3E, 0x3E, 0x3B, 0x3B, 0x3B, 0x3B, 0x2D, 0x2D, 0x2D, 0x2D, 0x34, 0x34, 0x34, 0x34,
    0x12, 0x12, 0x12, 0x12, 0x08, 0x08, 0x08, 0x08, 0x21, 0x21, 0x21, 0x21, 0x05, 0x05, 0x05, 0x05,
    0x16, 0x16, 0x16, 0x16, 0x19, 0x19, 0x19, 0x19, 0x26, 0x26, 0x26, 0x26, 0x1A, 0x1A, 0x1A, 0x1A,
    0x29, 0x29, 0x29, 0x29, 0x24, 0x24, 0x24, 0x24, 0x13, 0x13, 0x13, 0x13, 0x0F, 0x0F, 0x0F, 0x0F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3C, 0x3C, 0x3C, 0x3C, 0x33, 0x33, 0x33, 0x33, 0x0D, 0x0D, 0x0D, 0x0D,
    0x37, 0x37, 0x37, 0x37, 0x1C, 0x1C, 0x1C, 0x1C, 0x30, 0x30, 0x30, 0x30, 0x03, 0x03, 0x03, 0x03,
    0x0F, 0x0F, 0x0F, 0x0F, 0x3E, 0x3E, 0x3E, 0x3E, 0x3A, 0x3A, 0x3A, 0x3A, 0x2B, 0x2B, 0x2B, 0x2B,
    0x2C, 0x2C, 0x2C, 0x2C, 0x33, 0x33, 0x33, 0x33, 0x0C, 0x0C, 0x0C, 0x0C, 0x31, 0x31, 0x31, 0x31,
    0x05, 0x05, 0x05, 0x05, 0x15, 0x15, 0x15, 0x15, 0x17, 0x17, 0x17, 0x17, 0x1F, 0x1F, 0x1F, 0x1F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3D, 0x3D, 0x3D, 0x3D, 0x34, 0x34, 0x34, 0x34, 0x10, 0x10, 0x10, 0x10,
    0x02, 0x02, 0x02, 0x02, 0x08, 0x08, 0x08, 0x08, 0x23, 0x23, 0x23, 0x23, 0x0F, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x00, 0x0F, 0x0F, 0x00, 0x0F, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x0F, 0x00, 0x0F, 0x0F,
    0x0F, 0x0F, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x0F, 0x00,
    0x0F, 0x0F, 0x0F, 0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x00,
    0x0F, 0x0F, 0x00, 0x0F, 0x0F, 0x00, 0x0F, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x0F, 0x00, 0x0F, 0x0F,
    0x0F, 0x0F, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x0F, 0x00,
    0x0F, 0x0F, 0x0F, 0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00,
    0x0F, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00,
    0x0F, 0x0F, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00,
    0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00,
    0x0F, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00,
    0x0F, 0x0F, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x0F, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F, 0x3F, 0x3F,
    0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x20, 0x20, 0x20, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x20, 0x20, 0x20, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00, 0x0F, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0xCC, 0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33, 0x33,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03,
    0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03, 0x0A, 0x05, 0x0C, 0x03
};

_Static_assert(sizeof(EmcRamTrainingPatternData) % sizeof(EmcRamTrainingPattern) == 0, "Training pattern data must be a multiple of EmcRamTrainingPattern size");

static inline const EmcRamTrainingPattern *GetEmcRamTrainingPattern(void) {
    return (const EmcRamTrainingPattern *)(const void *)EmcRamTrainingPatternData;
}

#define DECLARE_REGISTER_HANDLER(BASE, REG, NAME) (BASE + REG),

static const uint32_t BurstRegisters[] = {
    FOREACH_BURST_REG(DECLARE_REGISTER_HANDLER)
};

static const uint32_t TrimRegisters[] = {
    FOREACH_TRIM_REG(DECLARE_REGISTER_HANDLER)
};

static const uint32_t BurstMcRegisters[] = {
    FOREACH_BURST_MC_REG(DECLARE_REGISTER_HANDLER)
};

static const uint32_t LaScaleRegisters[] = {
    FOREACH_LA_SCALE_REG(DECLARE_REGISTER_HANDLER)
};

static const uint32_t PerChannelTrimRegisters[] = {
    FOREACH_PER_CHANNEL_TRIM_REG(DECLARE_REGISTER_HANDLER)
};

static const uint32_t PerChannelBurstRegisters[] = {
    FOREACH_PER_CHANNEL_BURST_REG(DECLARE_REGISTER_HANDLER)
};

static const uint32_t PerChannelVrefRegisters[] = {
    FOREACH_PER_CHANNEL_VREF_REG(DECLARE_REGISTER_HANDLER)
};

static const uint32_t PerChannelTrainingModRegisters[] = {
    FOREACH_PER_CHANNEL_TRAINING_MOD_REG(DECLARE_REGISTER_HANDLER)
};

#undef DECLARE_REGISTER_HANDLER


static inline uint32_t u32_max(uint32_t a, uint32_t b) { return a > b ? a : b; }
static inline int       int_max(int a, int b)           { return a > b ? a : b; }
static inline int       int_min(int a, int b)           { return a < b ? a : b; }

static EmcDvfsTimingTable *GetEmcDvfsTimingTables(int index, void *mtc_tables_buffer) {
    const uint8_t *cmp_table;
    size_t         cmp_table_size;

    switch (index) {
#define HANDLE_CASE(N, TABLE)            \
        case N:                          \
            cmp_table      = TABLE;      \
            cmp_table_size = sizeof(TABLE); \
            break;
           HANDLE_CASE(0x00, T210b01SdevEmcDvfsTableS4gb01)
           HANDLE_CASE(0x05, T210b01SdevEmcDvfsTableS4gb03)
           HANDLE_CASE(0x06, T210b01SdevEmcDvfsTableS8gb03)
           HANDLE_CASE(0x07, T210b01SdevEmcDvfsTableH4gb03)
           HANDLE_CASE(0x08, T210b01SdevEmcDvfsTableM4gb03)
           HANDLE_CASE(0x09, T210b01SdevEmcDvfsTableS4gbY01)
           HANDLE_CASE(0x0A, T210b01SdevEmcDvfsTableS1y4gbY01)
           HANDLE_CASE(0x0B, T210b01SdevEmcDvfsTableS1y8gbY01)
           HANDLE_CASE(0x0C, T210b01SdevEmcDvfsTableS1y4gbX03)
           HANDLE_CASE(0x0D, T210b01SdevEmcDvfsTableS1y8gbX03)
           HANDLE_CASE(0x0E, T210b01SdevEmcDvfsTableS1y4gb01)
           HANDLE_CASE(0x0F, T210b01SdevEmcDvfsTableM1y4gb01)
           HANDLE_CASE(0x10, T210b01SdevEmcDvfsTableH1y4gb01)
           HANDLE_CASE(0x11, T210b01SdevEmcDvfsTableS1y8gb04)
           HANDLE_CASE(0x12, T210b01SdevEmcDvfsTableS1z4gb01)
           HANDLE_CASE(0x13, T210b01SdevEmcDvfsTableH1a4gb01)
           HANDLE_CASE(0x14, T210b01SdevEmcDvfsTableM1a4gb01)
#undef HANDLE_CASE
        default:
            EPRINTF("Unknown EmcDvfsTimingTableIndex");
            return NULL;
    }

    EmcDvfsTimingTable *out_tables = (EmcDvfsTimingTable *)mtc_tables_buffer;
    const size_t table_size = 2 * sizeof(EmcDvfsTimingTable);

    static u8 base_buffer[2 * sizeof(EmcDvfsTimingTable)];
    Lz4Uncompress(base_buffer, table_size, T210b01SdevEmcDvfsTableBase, sizeof(T210b01SdevEmcDvfsTableBase));
    Lz4Uncompress(out_tables, table_size, cmp_table, cmp_table_size);

    u8 *out_bytes = (u8 *)out_tables;
    for (size_t i = 0; i < table_size; i++)
        out_bytes[i] ^= base_buffer[i];

    return out_tables;
}

static bool IsSamePll(uint32_t next_2x, uint32_t prev_2x) {
    if (next_2x == prev_2x)
        return true;
    if ((next_2x == PLLM_OUT0 || next_2x == PLLM_UD) &&
        (prev_2x == PLLM_OUT0 || prev_2x == PLLM_UD))
        return true;
    return false;
}

static bool PllReprogram(uint32_t next_rate_khz, uint32_t next_clk_src,
                          uint32_t prev_rate_khz, uint32_t prev_clk_src) {
    /* Get current divp value. */
    uint32_t pll_p;
    switch (RegGetValue(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC,
                        CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC))) {
        case PLLM_UD:
        case PLLM_OUT0:
            pll_p = RegGetValue(CLKRST + CLK_RST_CONTROLLER_PLLM_BASE,
                                CLK_RST_REG_BITS_MASK(PLLM_BASE_PLLM_DIVP_B01));
            break;
        case PLLMB_UD:
        case PLLMB_OUT0:
            pll_p = RegGetValue(CLKRST + CLK_RST_CONTROLLER_PLLMB_BASE,
                                CLK_RST_REG_BITS_MASK(PLLMB_BASE_PLLMB_DIVP_B01));
            break;
        default:
            pll_p = 0;
            break;
    }

    const uint32_t next_2x  = RegGetField(next_clk_src, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC));
    const uint32_t prev_2x  = RegGetField(prev_clk_src, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC));
    uint32_t next_div = RegGetField(next_clk_src, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_DIVISOR));
    uint32_t prev_div = RegGetField(prev_clk_src, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_DIVISOR));

    if (next_2x == PLLM_UD  || next_2x == PLLMB_UD)  next_div = 0;
    if (prev_2x == PLLM_UD  || prev_2x == PLLMB_UD)  prev_div = 0;

    if (!IsSamePll(next_2x, prev_2x))
        return true;

    const float next_freq = (float)next_rate_khz * (1.0f + (next_div >> 1) + (0.5f * (next_div & 1))) * (float)(pll_p + 1);
    const float prev_freq = (float)prev_rate_khz * (1.0f + (prev_div >> 1) + (0.5f * (prev_div & 1))) * (float)(pll_p + 1);
    const float ratio     = prev_freq / next_freq;

    return ratio > 1.01f || ratio < 0.99f;
}

static uint32_t ProgramPllm(uint32_t next_rate_khz, uint32_t next_clk_src,
                              uint32_t ret_clk_src,   bool is_pllmb,
                              EmcDvfsTimingTable *timing) {
    uint32_t ret = ret_clk_src;
    (void)next_rate_khz; (void)next_clk_src;

    const uint32_t base = ((timing->pllmb_divm & 0xFF)       |
                           ((timing->pllmb_divn & 0xFF) << 8) |
                           ((timing->pllmb_divp & 1)    << 20));

    if (is_pllmb) {
        RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLMB_BASE, base);
        RegRead(CLKRST + CLK_RST_CONTROLLER_PLLMB_BASE);

        RegSetBits(CLKRST + CLK_RST_CONTROLLER_PLLMB_MISC1, 0x10000000);

        if (timing->pll_en_ssc & 1) {
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLMB_SS_CFG,   timing->pllmb_ss_cfg);
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLMB_SS_CTRL1, timing->pllmb_ss_ctrl1);
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLMB_SS_CTRL2, timing->pllmb_ss_ctrl2);
        } else {
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLMB_SS_CFG,   timing->pllmb_ss_cfg   & 0xBFFFFFFF);
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLMB_SS_CTRL2, timing->pllmb_ss_ctrl2 & 0x0000FFFF);
        }

        RegSetBits(CLKRST + CLK_RST_CONTROLLER_PLLMB_BASE, 0x40000000);

        switch (RegGetField(ret, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC))) {
            case PLLM_OUT0:
                ret = RegSetField(ret, CLK_RST_REG_BITS_VALUE(CLK_SOURCE_EMC_EMC_2X_CLK_SRC, PLLMB_OUT0));
                break;
            case PLLM_UD:
                ret = RegSetField(ret, CLK_RST_REG_BITS_VALUE(CLK_SOURCE_EMC_EMC_2X_CLK_SRC, PLLMB_UD));
                break;
        }

        while ((RegRead(CLKRST + CLK_RST_CONTROLLER_PLLMB_BASE) & 0x8000000) == 0) { /* wait */ }

    } else {
        RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLM_BASE, base);
        RegRead(CLKRST + CLK_RST_CONTROLLER_PLLM_BASE);

        RegSetBits(CLKRST + CLK_RST_CONTROLLER_PLLM_MISC2, 0x10);

        if (timing->pll_en_ssc & 1) {
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLM_SS_CFG,   timing->pllm_ss_cfg);
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLM_SS_CTRL1, timing->pllm_ss_ctrl1);
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLM_SS_CTRL2, timing->pllm_ss_ctrl2);
        } else {
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLM_SS_CFG,   timing->pllm_ss_cfg   & 0xBFFFFFFF);
            RegWrite(CLKRST + CLK_RST_CONTROLLER_PLLM_SS_CTRL2, timing->pllm_ss_ctrl2 & 0x0000FFFF);
        }

        RegSetBits(CLKRST + CLK_RST_CONTROLLER_PLLM_BASE, 0x40000000);

        switch (RegGetField(ret, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC))) {
            case PLLM_OUT0:
                ret = RegSetField(ret, CLK_RST_REG_BITS_VALUE(CLK_SOURCE_EMC_EMC_2X_CLK_SRC, PLLM_OUT0));
                break;
            case PLLM_UD:
                ret = RegSetField(ret, CLK_RST_REG_BITS_VALUE(CLK_SOURCE_EMC_EMC_2X_CLK_SRC, PLLM_UD));
                break;
        }

        while ((RegRead(CLKRST + CLK_RST_CONTROLLER_PLLM_BASE) & 0x8000000) == 0) { /* wait */ }
    }

    return ret;
}

static uint32_t GetDllState(EmcDvfsTimingTable *timing) {
    return (!(timing->emc_emrs & 0x1)) ? DLL_ON : DLL_OFF;
}

static int WaitForUpdate(uint32_t reg_offset, uint32_t mask, bool updated, uint32_t fbio_cfg7) {
    const int StatusUpdateTimeout = 1000;
    int result = 0;

    if (RegGetField(fbio_cfg7, EMC_REG_BITS_MASK(FBIO_CFG7_CH0_ENABLE)) == EMC_FBIO_CFG7_CH0_ENABLE_ENABLE) {
        bool success = false;
        for (int i = 0; i < StatusUpdateTimeout; ++i) {
            if (((RegRead(EMC0 + reg_offset) & mask) != 0) == updated) {
                success = true;
                break;
            }
            usleep(1);
        }
        result |= success ? 0 : 4;
    }

    if (RegGetField(fbio_cfg7, EMC_REG_BITS_MASK(FBIO_CFG7_CH1_ENABLE)) == EMC_FBIO_CFG7_CH1_ENABLE_ENABLE) {
        bool success = false;
        for (int i = 0; i < StatusUpdateTimeout; ++i) {
            if (((RegRead(EMC1 + reg_offset) & mask) != 0) == updated) {
                success = true;
                break;
            }
            usleep(1);
        }
        result |= success ? 0 : 4;
    }

    return result;
}

static void TimingUpdate(uint32_t fbio_cfg7) {
    RegWrite(EMC + EMC_TIMING_CONTROL, 1);
    WaitForUpdate(EMC_EMC_STATUS, 0x800000, false, fbio_cfg7);
}

static void CcfifoWrite(uint32_t addr, uint32_t data, uint32_t wait) {
    RegWrite(EMC + EMC_CCFIFO_DATA, data);
    RegWrite(EMC + EMC_CCFIFO_ADDR, (addr & 0xFFFF) | ((wait & 0x7FFF) << 16) | 0x80000000);
}

static uint32_t ActualOscClocks(uint32_t in) {
    if      (in < 0x40) return in * 0x10;
    else if (in < 0x80) return 0x800;
    else if (in < 0xC0) return 0x1000;
    else                return 0x2000;
}

static uint32_t DivideUpFloat(uint32_t a, uint32_t b) {
    const float res   = (float)a / (float)b;
    const uint32_t fl = (uint32_t)res;
    return fl + (((float)fl + 0.01f < res) ? 1u : 0u);
}

static void StartPeriodicCompensation(void) {
    RegWrite(EMC + EMC_MPC, 0x4B);
    RegRead(EMC + EMC_MPC);
}

static uint32_t SetShadowBypass(uint32_t val, uint32_t emc_dbg) {
    emc_dbg = RegSetField(emc_dbg, EMC_REG_BITS_VALUE(DBG_WRITE_MUX, val));
    return emc_dbg;
}

static uint32_t g_periodic_timmer_compensation_intermediates[9 * 0x10];

static uint32_t ApplyPeriodicCompensationTrimmer(EmcDvfsTimingTable *timing, uint32_t trim_reg) {
    uint32_t rate_mhz = timing->rate_khz / 1000;
    uint32_t adj[0x10] = { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 };

    int      tree_delta[4]      = {0};
    uint32_t tree_delta_taps[4] = {0};

#define SET_TRIM_INTERMEDIATE(_arr_, _emc_, _rank_, _byte_)                                                   \
    do {                                                                                                       \
        const uint32_t shft = timing->trim_perch_regs.emc ## _emc_ ## _data_brlshft_ ## _rank_;              \
        const uint32_t base = ((shft >> (3 * (_byte_))) & 7) << 6;                                            \
        const uint32_t val0 = timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank ## _rank_ ## _byte ## _byte_ ## _0; \
        const uint32_t val1 = timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank ## _rank_ ## _byte ## _byte_ ## _1; \
        const uint32_t val2 = timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank ## _rank_ ## _byte ## _byte_ ## _2; \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 0] = base + ((val0 >>  0) & 0xFF);                              \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 1] = base + ((val0 >>  8) & 0xFF);                              \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 2] = base + ((val0 >> 16) & 0xFF);                              \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 3] = base + ((val0 >> 24) & 0xFF);                              \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 4] = base + ((val1 >>  0) & 0xFF);                              \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 5] = base + ((val1 >>  8) & 0xFF);                              \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 6] = base + ((val1 >> 16) & 0xFF);                              \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 7] = base + ((val1 >> 24) & 0xFF);                              \
        _arr_[9 * (8 * (_rank_) + (_byte_)) + 8] = base + ((val2 >>  0) & 0xFF);                              \
    } while (0)

    {
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 0, 0, 0);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 0, 0, 1);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 0, 0, 2);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 0, 0, 3);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 1, 0, 4);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 1, 0, 5);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 1, 0, 6);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 1, 0, 7);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 0, 1, 0);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 0, 1, 1);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 0, 1, 2);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 0, 1, 3);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 1, 1, 4);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 1, 1, 5);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 1, 1, 6);
        SET_TRIM_INTERMEDIATE(g_periodic_timmer_compensation_intermediates, 1, 1, 7);
    }
#undef SET_TRIM_INTERMEDIATE

    switch (trim_reg) {
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_2:
        case EMC0_BASE + EMC_DATA_BRLSHFT_0:
        case EMC1_BASE + EMC_DATA_BRLSHFT_0:
        {
            tree_delta[0] = 128 * (timing->current_dram_clktree_c0d0u0 - timing->trained_dram_clktree_c0d0u0);
            tree_delta[1] = 128 * (timing->current_dram_clktree_c0d0u1 - timing->trained_dram_clktree_c0d0u1);
            tree_delta[2] = 128 * (timing->current_dram_clktree_c1d0u0 - timing->trained_dram_clktree_c1d0u0);
            tree_delta[3] = 128 * (timing->current_dram_clktree_c1d0u1 - timing->trained_dram_clktree_c1d0u1);
            tree_delta_taps[0] = (uint32_t)((tree_delta[0] * (int)rate_mhz) / 1000000);
            tree_delta_taps[1] = (uint32_t)((tree_delta[1] * (int)rate_mhz) / 1000000);
            tree_delta_taps[2] = (uint32_t)((tree_delta[2] * (int)rate_mhz) / 1000000);
            tree_delta_taps[3] = (uint32_t)((tree_delta[3] * (int)rate_mhz) / 1000000);

            for (int i = 0; i < 4; ++i) {
                const uint32_t sum = (tree_delta_taps[i] <= timing->tree_margin) ? 0 : tree_delta_taps[i];
                for (int j = 0; j < 18; ++j) {
                    const uint32_t v = (g_periodic_timmer_compensation_intermediates[18 * i + j] += sum);
                    if (v < (adj[2 * i + (j < 9 ? 1 : 0)] << 6)) {
                        adj[2 * i + (j < 9 ? 1 : 0)] = v >> 6;
                    }
                }
                for (int j = 0; j < 18; ++j) {
                    g_periodic_timmer_compensation_intermediates[18 * i + j] -= (adj[2 * i + (j < 9 ? 1 : 0)] << 6);
                }
            }
        }
        break;

        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_2:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_0:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_1:
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_2:
        case EMC0_BASE + EMC_DATA_BRLSHFT_1:
        case EMC1_BASE + EMC_DATA_BRLSHFT_1:
        {
            tree_delta[0] = 128 * (timing->current_dram_clktree_c0d1u0 - timing->trained_dram_clktree_c0d1u0);
            tree_delta[1] = 128 * (timing->current_dram_clktree_c0d1u1 - timing->trained_dram_clktree_c0d1u1);
            tree_delta[2] = 128 * (timing->current_dram_clktree_c1d1u0 - timing->trained_dram_clktree_c1d1u0);
            tree_delta[3] = 128 * (timing->current_dram_clktree_c1d1u1 - timing->trained_dram_clktree_c1d1u1);
            tree_delta_taps[0] = (uint32_t)((tree_delta[0] * (int)rate_mhz) / 1000000);
            tree_delta_taps[1] = (uint32_t)((tree_delta[1] * (int)rate_mhz) / 1000000);
            tree_delta_taps[2] = (uint32_t)((tree_delta[2] * (int)rate_mhz) / 1000000);
            tree_delta_taps[3] = (uint32_t)((tree_delta[3] * (int)rate_mhz) / 1000000);

            for (int i = 0; i < 4; ++i) {
                const uint32_t sum = (tree_delta_taps[i] <= timing->tree_margin) ? 0 : tree_delta_taps[i];
                for (int j = 0; j < 18; ++j) {
                    const uint32_t v = (g_periodic_timmer_compensation_intermediates[72 + 18 * i + j] += sum);
                    if (v < (adj[8 + 2 * i + (j < 9 ? 1 : 0)] << 6)) {
                        adj[8 + 2 * i + (j < 9 ? 1 : 0)] = v >> 6;
                    }
                }
                for (int j = 0; j < 18; ++j) {
                    g_periodic_timmer_compensation_intermediates[72 + 18 * i + j] -= (adj[8 + 2 * i + (j < 9 ? 1 : 0)] << 6);
                }
            }
        }
        break;

        default:
            break;
    }

    uint32_t result = 0;
    switch (trim_reg) {
        case EMC0_BASE + EMC_DATA_BRLSHFT_0:
            result = ((adj[ 0] & 7) <<  0) | ((adj[ 1] & 7) <<  3) | ((adj[ 2] & 7) <<  6) | ((adj[ 3] & 7) <<  9);
            break;
        case EMC1_BASE + EMC_DATA_BRLSHFT_0:
            result = ((adj[ 4] & 7) << 12) | ((adj[ 5] & 7) << 15) | ((adj[ 6] & 7) << 18) | ((adj[ 7] & 7) << 21);
            break;
        case EMC0_BASE + EMC_DATA_BRLSHFT_1:
            result = ((adj[ 8] & 7) <<  0) | ((adj[ 9] & 7) <<  3) | ((adj[10] & 7) <<  6) | ((adj[11] & 7) <<  9);
            break;
        case EMC1_BASE + EMC_DATA_BRLSHFT_1:
            result = ((adj[12] & 7) << 12) | ((adj[13] & 7) << 15) | ((adj[14] & 7) << 18) | ((adj[15] & 7) << 21);
            break;

#define ADD_TRIM_CASE(_ARR_, _RANK_, _BYTE_)                                                                                                                                                                                                                   \
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK ## _RANK_ ## _BYTE ## _BYTE_ ## _0:                                                                                                                                                                  \
            result = ((_ARR_[9*(8*(_RANK_)+(_BYTE_))+0] & 0xFF) << 0) | ((_ARR_[9*(8*(_RANK_)+(_BYTE_))+1] & 0xFF) << 8)  | ((_ARR_[9*(8*(_RANK_)+(_BYTE_))+2] & 0xFF) << 16) | ((_ARR_[9*(8*(_RANK_)+(_BYTE_))+3] & 0xFF) << 24); \
            break;                                                                                                                                                                                                                                             \
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK ## _RANK_ ## _BYTE ## _BYTE_ ## _1:                                                                                                                                                                  \
            result = ((_ARR_[9*(8*(_RANK_)+(_BYTE_))+4] & 0xFF) << 0) | ((_ARR_[9*(8*(_RANK_)+(_BYTE_))+5] & 0xFF) << 8)  | ((_ARR_[9*(8*(_RANK_)+(_BYTE_))+6] & 0xFF) << 16) | ((_ARR_[9*(8*(_RANK_)+(_BYTE_))+7] & 0xFF) << 24); \
            break;                                                                                                                                                                                                                                             \
        case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK ## _RANK_ ## _BYTE ## _BYTE_ ## _2:                                                                                                                                                                  \
            result = (_ARR_[9*(8*(_RANK_)+(_BYTE_))+8] & 0xFF);                                                                                                                                                                                               \
            break;

        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 0, 0)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 0, 1)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 0, 2)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 0, 3)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 0, 4)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 0, 5)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 0, 6)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 0, 7)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 1, 0)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 1, 1)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 1, 2)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 1, 3)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 1, 4)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 1, 5)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 1, 6)
        ADD_TRIM_CASE(g_periodic_timmer_compensation_intermediates, 1, 7)
#undef ADD_TRIM_CASE
    }

    return result;
}

#define CVAL(v) ((uint32_t)((1000 * ((1000 * ActualOscClocks(src_timing->run_clocks)) / current_timing_rate_mhz)) / (2 * (v))))

static uint32_t UpdateClockTreeDelay(EmcDvfsTimingTable *src_timing,
                                      EmcDvfsTimingTable *dst_timing,
                                      uint32_t dram_dev_num,
                                      uint32_t mode,
                                      int type) {
    uint32_t mrr_req = 0, mrr_data = 0;
    uint32_t temp0_0 = 0, temp0_1 = 0, temp1_0 = 0, temp1_1 = 0;
    int tdel = 0, tmdel = 0, adel = 0;

    uint32_t current_timing_rate_mhz = src_timing->rate_khz / 1000;
    uint32_t next_timing_rate_mhz    = dst_timing->rate_khz / 1000;
    uint32_t fbio_cfg7               = dst_timing->emc_fbio_cfg7;

    const bool ch0_enable = RegGetField(fbio_cfg7, EMC_REG_BITS_MASK(FBIO_CFG7_CH0_ENABLE)) == EMC_FBIO_CFG7_CH0_ENABLE_ENABLE;
    const bool ch1_enable = RegGetField(fbio_cfg7, EMC_REG_BITS_MASK(FBIO_CFG7_CH1_ENABLE)) == EMC_FBIO_CFG7_CH1_ENABLE_ENABLE;

    bool dvfs_pt1                 = (type == DVFS_PT1);
    bool training_pt1             = (type == TRAINING_PT1);
    bool dvfs_update              = (type == DVFS_UPDATE);
    bool training_update          = (type == TRAINING_UPDATE);
    bool periodic_training_update = (type == PERIODIC_TRAINING_UPDATE);

    /* Dev0 MSB */
    if (dvfs_pt1 || training_pt1 || periodic_training_update) {
        mrr_req = ((2u << 30) | (19 << 16));
        RegWrite(EMC + EMC_MRR, mrr_req);
        WaitForUpdate(EMC_EMC_STATUS, (1u << 20), true, fbio_cfg7);

        if (ch0_enable) {
            mrr_data = (RegRead(EMC0 + EMC_MRR) & 0xFFFF);
            temp0_0 = ((mrr_data & 0xff) << 8);
            temp0_1 = (mrr_data & 0xff00);
        }
        if (ch1_enable) {
            mrr_data = (RegRead(EMC1 + EMC_MRR) & 0xFFFF);
            temp1_0 = ((mrr_data & 0xff) << 8);
            temp1_1 = (mrr_data & 0xff00);
        }

        /* Dev0 LSB */
        mrr_req = ((mrr_req & ~(0xFFu << 16)) | (18 << 16));
        RegWrite(EMC + EMC_MRR, mrr_req);
        WaitForUpdate(EMC_EMC_STATUS, (1u << 20), true, fbio_cfg7);

        if (ch0_enable) {
            mrr_data = (RegRead(EMC0 + EMC_MRR) & 0xFFFF);
            temp0_0 |= (mrr_data & 0xff);
            temp0_1 |= (mrr_data & 0xff00) >> 8;
        }
        if (ch1_enable) {
            mrr_data = (RegRead(EMC1 + EMC_MRR) & 0xFFFF);
            temp1_0 |= (mrr_data & 0xff);
            temp1_1 |= (mrr_data & 0xff00) >> 8;
        }
    }

#undef __INCREMENT_PTFV
#undef __AVERAGE_PTFV
#undef __AVERAGE_WRITE_PTFV
#undef __WEIGHTED_UPDATE_PTFV
#undef __MOVAVG_AC

#define __INCREMENT_PTFV(field, val)    (dst_timing->ptfv_dqsosc_movavg_ ## field += (val))
#define __AVERAGE_PTFV(field)           (dst_timing->ptfv_dqsosc_movavg_ ## field /= dst_timing->ptfv_dvfs_samples)
#define __AVERAGE_WRITE_PTFV(field)     (dst_timing->ptfv_dqsosc_movavg_ ## field /= dst_timing->ptfv_write_samples)
#define __WEIGHTED_UPDATE_PTFV(field, val) \
    (dst_timing->ptfv_dqsosc_movavg_ ## field = \
        ((dst_timing->ptfv_dqsosc_movavg_ ## field * (dst_timing->ptfv_dvfs_samples - 1)) + (val)) \
        / dst_timing->ptfv_dvfs_samples)
#define __MOVAVG_AC(t, field) ((t)->ptfv_dqsosc_movavg_ ## field)

    if (ch0_enable) {
        if      (dvfs_pt1 || training_pt1)    __INCREMENT_PTFV(c0d0u0, CVAL(temp0_0));
        else if (dvfs_update)                  __AVERAGE_PTFV(c0d0u0);
        else if (training_update)              __AVERAGE_WRITE_PTFV(c0d0u0);
        else if (periodic_training_update)     __WEIGHTED_UPDATE_PTFV(c0d0u0, CVAL(temp0_0));

        if (dvfs_update || training_update || periodic_training_update) {
            tdel  = (dst_timing->current_dram_clktree_c0d0u0 - __MOVAVG_AC(dst_timing, c0d0u0));
            tmdel = (tdel < 0) ? ~tdel : tdel;
            adel  = tmdel;
            if (mode == 1 || ((adel * 128 * next_timing_rate_mhz) / 1000000) > dst_timing->tree_margin)
                dst_timing->current_dram_clktree_c0d0u0 = __MOVAVG_AC(dst_timing, c0d0u0);
        }

        if      (dvfs_pt1 || training_pt1)    __INCREMENT_PTFV(c0d0u1, CVAL(temp0_1));
        else if (dvfs_update)                  __AVERAGE_PTFV(c0d0u1);
        else if (training_update)              __AVERAGE_WRITE_PTFV(c0d0u1);
        else if (periodic_training_update)     __WEIGHTED_UPDATE_PTFV(c0d0u1, CVAL(temp0_1));

        if (dvfs_update || training_update || periodic_training_update) {
            tdel  = (dst_timing->current_dram_clktree_c0d0u1 - __MOVAVG_AC(dst_timing, c0d0u1));
            tmdel = (tdel < 0) ? -tdel : tdel;
            if (tmdel > adel) adel = tmdel;
            if (mode == 1 || ((tmdel * 128 * next_timing_rate_mhz) / 1000000) > dst_timing->tree_margin)
                dst_timing->current_dram_clktree_c0d0u1 = __MOVAVG_AC(dst_timing, c0d0u1);
        }
    } else {
        adel = 0;
    }

    if (ch1_enable) {
        if      (dvfs_pt1 || training_pt1)    __INCREMENT_PTFV(c1d0u0, CVAL(temp1_0));
        else if (dvfs_update)                  __AVERAGE_PTFV(c1d0u0);
        else if (training_update)              __AVERAGE_WRITE_PTFV(c1d0u0);
        else if (periodic_training_update)     __WEIGHTED_UPDATE_PTFV(c1d0u0, CVAL(temp1_0));

        if (dvfs_update || training_update || periodic_training_update) {
            tdel  = (dst_timing->current_dram_clktree_c1d0u0 - __MOVAVG_AC(dst_timing, c1d0u0));
            tmdel = (tdel < 0) ? -tdel : tdel;
            if (tmdel > adel) adel = tmdel;
            if (mode == 1 || ((tmdel * 128 * next_timing_rate_mhz) / 1000000) > dst_timing->tree_margin)
                dst_timing->current_dram_clktree_c1d0u0 = __MOVAVG_AC(dst_timing, c1d0u0);
        }

        if      (dvfs_pt1 || training_pt1)    __INCREMENT_PTFV(c1d0u1, CVAL(temp1_1));
        else if (dvfs_update)                  __AVERAGE_PTFV(c1d0u1);
        else if (training_update)              __AVERAGE_WRITE_PTFV(c1d0u1);
        else if (periodic_training_update)     __WEIGHTED_UPDATE_PTFV(c1d0u1, CVAL(temp1_1));

        if (dvfs_update || training_update || periodic_training_update) {
            tdel  = (dst_timing->current_dram_clktree_c1d0u1 - __MOVAVG_AC(dst_timing, c1d0u1));
            tmdel = (tdel < 0) ? -tdel : tdel;
            if (tmdel > adel) adel = tmdel;
            if (mode == 1 || ((tmdel * 128 * next_timing_rate_mhz) / 1000000) > dst_timing->tree_margin)
                dst_timing->current_dram_clktree_c1d0u1 = __MOVAVG_AC(dst_timing, c1d0u1);
        }
    }

    if (dram_dev_num == TWO_RANK) {
        /* Dev1 MSB */
        if (dvfs_pt1 || training_pt1 || periodic_training_update) {
            mrr_req = ((1u << 30) | (19 << 16));
            RegWrite(EMC + EMC_MRR, mrr_req);
            WaitForUpdate(EMC_EMC_STATUS, (1u << 20), true, fbio_cfg7);

            if (ch0_enable) {
                mrr_data = (RegRead(EMC0 + EMC_MRR) & 0xFFFF);
                temp0_0 = ((mrr_data & 0xff) << 8);
                temp0_1 = (mrr_data & 0xff00);
            }
            if (ch1_enable) {
                mrr_data = (RegRead(EMC1 + EMC_MRR) & 0xFFFF);
                temp1_0 = ((mrr_data & 0xff) << 8);
                temp1_1 = (mrr_data & 0xff00);
            }

            /* Dev1 LSB */
            mrr_req = ((mrr_req & ~(0xFFu << 16)) | (18 << 16));
            RegWrite(EMC + EMC_MRR, mrr_req);
            WaitForUpdate(EMC_EMC_STATUS, (1u << 20), true, fbio_cfg7);

            if (ch0_enable) {
                mrr_data = (RegRead(EMC0 + EMC_MRR) & 0xFFFF);
                temp0_0 |= ((mrr_data & 0xff) << 8);
                temp0_1 |= (mrr_data & 0xff00);
            }
            if (ch1_enable) {
                mrr_data = (RegRead(EMC1 + EMC_MRR) & 0xFFFF);
                temp1_0 |= ((mrr_data & 0xff) << 8);
                temp1_1 |= (mrr_data & 0xff00);
            }
        }

        if (ch0_enable) {
            if      (dvfs_pt1 || training_pt1)    __INCREMENT_PTFV(c0d1u0, CVAL(temp0_0));
            else if (dvfs_update)                  __AVERAGE_PTFV(c0d1u0);
            else if (training_update)              __AVERAGE_WRITE_PTFV(c0d1u0);
            else if (periodic_training_update)     __WEIGHTED_UPDATE_PTFV(c0d1u0, CVAL(temp0_0));

            if (dvfs_update || training_update || periodic_training_update) {
                tdel  = (dst_timing->current_dram_clktree_c0d1u0 - __MOVAVG_AC(dst_timing, c0d1u0));
                tmdel = (tdel < 0) ? -tdel : tdel;
                if (tmdel > adel) adel = tmdel;
                if (mode == 1 || ((tmdel * 128 * next_timing_rate_mhz) / 1000000) > dst_timing->tree_margin)
                    dst_timing->current_dram_clktree_c0d1u0 = __MOVAVG_AC(dst_timing, c0d1u0);
            }

            if      (dvfs_pt1 || training_pt1)    __INCREMENT_PTFV(c0d1u1, CVAL(temp0_1));
            else if (dvfs_update)                  __AVERAGE_PTFV(c0d1u1);
            else if (training_update)              __AVERAGE_WRITE_PTFV(c0d1u1);
            else if (periodic_training_update)     __WEIGHTED_UPDATE_PTFV(c0d1u1, CVAL(temp0_1));

            if (dvfs_update || training_update || periodic_training_update) {
                tdel  = (dst_timing->current_dram_clktree_c0d1u1 - __MOVAVG_AC(dst_timing, c0d1u1));
                tmdel = (tdel < 0) ? -tdel : tdel;
                if (tmdel > adel) adel = tmdel;
                if (mode == 1 || ((tmdel * 128 * next_timing_rate_mhz) / 1000000) > dst_timing->tree_margin)
                    dst_timing->current_dram_clktree_c0d1u1 = __MOVAVG_AC(dst_timing, c0d1u1);
            }
        }

        if (ch1_enable) {
            if      (dvfs_pt1 || training_pt1)    __INCREMENT_PTFV(c1d1u0, CVAL(temp1_0));
            else if (dvfs_update)                  __AVERAGE_PTFV(c1d1u0);
            else if (training_update)              __AVERAGE_WRITE_PTFV(c1d1u0);
            else if (periodic_training_update)     __WEIGHTED_UPDATE_PTFV(c1d1u0, CVAL(temp1_0));

            if (dvfs_update || training_update || periodic_training_update) {
                tdel  = (dst_timing->current_dram_clktree_c1d1u0 - __MOVAVG_AC(dst_timing, c1d1u0));
                tmdel = (tdel < 0) ? -tdel : tdel;
                if (tmdel > adel) adel = tmdel;
                if (mode == 1 || ((tmdel * 128 * next_timing_rate_mhz) / 1000000) > dst_timing->tree_margin)
                    dst_timing->current_dram_clktree_c1d1u0 = __MOVAVG_AC(dst_timing, c1d1u0);
            }

            if      (dvfs_pt1 || training_pt1)    __INCREMENT_PTFV(c1d1u1, CVAL(temp1_1));
            else if (dvfs_update)                  __AVERAGE_PTFV(c1d1u1);
            else if (training_update)              __AVERAGE_WRITE_PTFV(c1d1u1);
            else if (periodic_training_update)     __WEIGHTED_UPDATE_PTFV(c1d1u1, CVAL(temp1_1));

            if (dvfs_update || training_update || periodic_training_update) {
                tdel  = (dst_timing->current_dram_clktree_c1d1u1 - __MOVAVG_AC(dst_timing, c1d1u1));
                tmdel = (tdel < 0) ? -tdel : tdel;
                if (tmdel > adel) adel = tmdel;
                if (mode == 1 || ((tmdel * 128 * next_timing_rate_mhz) / 1000000) > dst_timing->tree_margin)
                    dst_timing->current_dram_clktree_c1d1u1 = __MOVAVG_AC(dst_timing, c1d1u1);
            }
        }
    }

#undef CVAL
#undef __INCREMENT_PTFV
#undef __AVERAGE_PTFV
#undef __AVERAGE_WRITE_PTFV
#undef __WEIGHTED_UPDATE_PTFV
#undef __MOVAVG_AC

    if (mode == 1) {
        dst_timing->trained_dram_clktree_c0d0u0 = dst_timing->current_dram_clktree_c0d0u0;
        dst_timing->trained_dram_clktree_c0d0u1 = dst_timing->current_dram_clktree_c0d0u1;
        dst_timing->trained_dram_clktree_c0d1u0 = dst_timing->current_dram_clktree_c0d1u0;
        dst_timing->trained_dram_clktree_c0d1u1 = dst_timing->current_dram_clktree_c0d1u1;
        dst_timing->trained_dram_clktree_c1d0u0 = dst_timing->current_dram_clktree_c1d0u0;
        dst_timing->trained_dram_clktree_c1d0u1 = dst_timing->current_dram_clktree_c1d0u1;
        dst_timing->trained_dram_clktree_c1d1u0 = dst_timing->current_dram_clktree_c1d1u0;
        dst_timing->trained_dram_clktree_c1d1u1 = dst_timing->current_dram_clktree_c1d1u1;
    }

    return (uint32_t)adel;
}

static uint32_t PeriodicCompensationHandler(int type, uint32_t dram_dev_num,
                                              EmcDvfsTimingTable *src_timing,
                                              EmcDvfsTimingTable *dst_timing) {
    if (!dst_timing->periodic_training)
        return 0;

    uint32_t adel          = 0;
    uint32_t samples       = dst_timing->ptfv_dvfs_samples;
    uint32_t samples_write = dst_timing->ptfv_write_samples;
    uint32_t delay         = 2 + (1000 * ActualOscClocks(src_timing->run_clocks) / src_timing->rate_khz);

    if (type == DVFS_SEQUENCE) {
        if (src_timing->periodic_training && (dst_timing->ptfv_config_ctrl & 1)) {
            dst_timing->ptfv_dqsosc_movavg_c0d0u0 = src_timing->ptfv_dqsosc_movavg_c0d0u0 * samples;
            dst_timing->ptfv_dqsosc_movavg_c0d0u1 = src_timing->ptfv_dqsosc_movavg_c0d0u1 * samples;
            dst_timing->ptfv_dqsosc_movavg_c1d0u0 = src_timing->ptfv_dqsosc_movavg_c1d0u0 * samples;
            dst_timing->ptfv_dqsosc_movavg_c1d0u1 = src_timing->ptfv_dqsosc_movavg_c1d0u1 * samples;
            dst_timing->ptfv_dqsosc_movavg_c0d1u0 = src_timing->ptfv_dqsosc_movavg_c0d1u0 * samples;
            dst_timing->ptfv_dqsosc_movavg_c0d1u1 = src_timing->ptfv_dqsosc_movavg_c0d1u1 * samples;
            dst_timing->ptfv_dqsosc_movavg_c1d1u0 = src_timing->ptfv_dqsosc_movavg_c1d1u0 * samples;
            dst_timing->ptfv_dqsosc_movavg_c1d1u1 = src_timing->ptfv_dqsosc_movavg_c1d1u1 * samples;
        } else {
            dst_timing->ptfv_dqsosc_movavg_c0d0u0 = 0;
            dst_timing->ptfv_dqsosc_movavg_c0d0u1 = 0;
            dst_timing->ptfv_dqsosc_movavg_c0d1u0 = 0;
            dst_timing->ptfv_dqsosc_movavg_c0d1u1 = 0;
            dst_timing->ptfv_dqsosc_movavg_c1d0u0 = 0;
            dst_timing->ptfv_dqsosc_movavg_c1d0u1 = 0;
            dst_timing->ptfv_dqsosc_movavg_c1d1u0 = 0;
            dst_timing->ptfv_dqsosc_movavg_c1d1u1 = 0;

            for (uint32_t i = 0; i < samples; ++i) {
                StartPeriodicCompensation();
                usleep(delay);
                adel = UpdateClockTreeDelay(src_timing, dst_timing, dram_dev_num, 0, DVFS_PT1);
            }
        }
        adel = UpdateClockTreeDelay(src_timing, dst_timing, dram_dev_num, 0, DVFS_UPDATE);

    } else if (type == WRITE_TRAINING_SEQUENCE) {
        dst_timing->ptfv_dqsosc_movavg_c0d0u0 = 0;
        dst_timing->ptfv_dqsosc_movavg_c0d0u1 = 0;
        dst_timing->ptfv_dqsosc_movavg_c0d1u0 = 0;
        dst_timing->ptfv_dqsosc_movavg_c0d1u1 = 0;
        dst_timing->ptfv_dqsosc_movavg_c1d0u0 = 0;
        dst_timing->ptfv_dqsosc_movavg_c1d0u1 = 0;
        dst_timing->ptfv_dqsosc_movavg_c1d1u0 = 0;
        dst_timing->ptfv_dqsosc_movavg_c1d1u1 = 0;

        for (uint32_t i = 0; i < samples_write; ++i) {
            StartPeriodicCompensation();
            usleep(delay);
            adel = UpdateClockTreeDelay(src_timing, dst_timing, dram_dev_num, 1, TRAINING_PT1);
        }
        adel = UpdateClockTreeDelay(src_timing, dst_timing, dram_dev_num, 1, TRAINING_UPDATE);

    } else if (type == PERIODIC_TRAINING_SEQUENCE) {
        StartPeriodicCompensation();
        usleep(delay);
        adel = UpdateClockTreeDelay(src_timing, dst_timing, dram_dev_num, 0, PERIODIC_TRAINING_UPDATE);
    }

    return adel;
}

static void ChangeDllSrc(EmcDvfsTimingTable *dst_timing, uint32_t next_clk_src) {
    uint32_t dll_setting = ((next_clk_src & 0xE00000FF) | (dst_timing->dll_clk_src & 0x1FFFFF00)) & 0xFFFFF3FF;

    switch (RegGetField(next_clk_src, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC))) {
        case PLLMB_UD:
            dll_setting |= 0x400; /* PLLM_VCOB */
            break;
        case PLLM_UD:
            dll_setting |= 0x000; /* PLLM_VCOA */
            break;
        default:
            dll_setting |= 0x800; /* EMC_DLL_SWITCH_OUT */
            break;
    }

    RegWrite(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL, dll_setting);

    RegReadWriteField(CLKRST + CLK_RST_CONTROLLER_CLK_OUT_ENB_X,
                 CLK_RST_REG_BITS_ENUM_SEL(CLK_ENB_X_CLK_ENB_EMC_DLL,
                     (dst_timing->clk_out_enb_x_0_clk_enb_emc_dll & 1),
                     ENABLE, DISABLE));
    RegRead(CLKRST + CLK_RST_CONTROLLER_CLK_OUT_ENB_X);
}

static void DllPrelock(EmcDvfsTimingTable *dst_timing, EmcDvfsTimingTable *src_timing,
                        bool training_enabled, uint32_t next_clk_src) {
    (void)src_timing;

    RegWrite(EMC + EMC_CFG_DIG_DLL, (RegRead(EMC + EMC_CFG_DIG_DLL) & 0xFFFFFFE4) | 0x00000008);
    TimingUpdate(dst_timing->emc_fbio_cfg7);

    RegWrite(EMC + EMC_CFG_DIG_DLL, (RegRead(EMC + EMC_CFG_DIG_DLL) & 0xFFFFF824) | 0x000003C8);
    TimingUpdate(dst_timing->emc_fbio_cfg7);

    WaitForUpdate(EMC_CFG_DIG_DLL, (1 << 0), false, dst_timing->emc_fbio_cfg7);

    RegWrite(EMC + EMC_PMACRO_DLL_CFG_0, dst_timing->burst_regs.emc_pmacro_dll_cfg_0);
    RegRead(EMC + EMC_PMACRO_DLL_CFG_1);

    RegWrite(EMC + EMC_PMACRO_DLL_CFG_1,
             (dst_timing->burst_regs.emc_pmacro_dll_cfg_1 & 0xFFFFDFFF) |
             (RegRead(EMC + EMC_PMACRO_DLL_CFG_1) & 0x00002000));

    TimingUpdate(dst_timing->emc_fbio_cfg7);

    ChangeDllSrc(dst_timing, next_clk_src);
    usleep(2);

    RegSetBits(EMC + EMC_CFG_DIG_DLL, 0x1);
    TimingUpdate(dst_timing->emc_fbio_cfg7);

    WaitForUpdate(EMC_CFG_DIG_DLL,    (1 << 0), true,  dst_timing->emc_fbio_cfg7);
    WaitForUpdate(EMC_DIG_DLL_STATUS, (1 << 2), true,  dst_timing->emc_fbio_cfg7);

    if (training_enabled) {
        RegSetBits(EMC + EMC_DBG, 0x2);
        RegClearBits(EMC + EMC_CFG_DIG_DLL, 0x1);
        RegClearBits(EMC + EMC_DBG, 0x2);
        WaitForUpdate(EMC_CFG_DIG_DLL, (1 << 0), false, dst_timing->emc_fbio_cfg7);
    }

    RegRead(EMC + EMC_PMACRO_DIG_DLL_STATUS_0);
}

static void DllDisable(uint32_t fbio_cfg7) {
    RegClearBits(EMC + EMC_CFG_DIG_DLL, 0x1);
    TimingUpdate(fbio_cfg7);
    WaitForUpdate(EMC_CFG_DIG_DLL, (1 << 0), false, fbio_cfg7);
}

static void PllDisable(uint32_t dst_clk_src) {
    switch (RegGetField(dst_clk_src, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC))) {
        case PLLM_OUT0:
        case PLLM_UD:
            RegClearBits(CLKRST + CLK_RST_CONTROLLER_PLLMB_BASE, 0x40000000);
            break;
        case PLLMB_OUT0:
        case PLLMB_UD:
            RegClearBits(CLKRST + CLK_RST_CONTROLLER_PLLM_BASE,  0x40000000);
            break;
        default:
            RegClearBits(CLKRST + CLK_RST_CONTROLLER_PLLMB_BASE, 0x40000000);
            RegClearBits(CLKRST + CLK_RST_CONTROLLER_PLLM_BASE,  0x40000000);
            break;
    }
}

static void DvfsPowerRampDown(EmcDvfsTimingTable *src_timing, EmcDvfsTimingTable *dst_timing,
                               bool flip_backward, uint32_t vtt_vdda_channel) {
    EmcDvfsTimingTable *from_table = flip_backward ? dst_timing : src_timing;
    EmcDvfsTimingTable *to_table   = flip_backward ? src_timing : dst_timing;

    uint32_t from_rate_khz = from_table->rate_khz;
    uint32_t from_period   = 1000000000u / from_rate_khz;
    uint32_t to_rate_khz   = to_table->rate_khz;
    uint32_t clk_div       = 1000 * dst_timing->src_clock_div;

    uint32_t delay = UtilDivideUp(clk_div, from_period);

    if (from_rate_khz >= 407997 || to_rate_khz <= 407996) {
        if (from_rate_khz >= 407997 && to_rate_khz <= 407996) {
            uint32_t pmacro_vttgen_ctrl_1 = RegRead(EMC + EMC_PMACRO_VTTGEN_CTRL_1);

            if (dst_timing->vtt_vdda_dual_channel) {
                if (vtt_vdda_channel != 1) return;
                CcfifoWrite(EMC_PMACRO_VTTGEN_CTRL_1,
                            (pmacro_vttgen_ctrl_1 & 0xFFFF03FF) | ((to_table->vtt_vdda_ctrl_4 & 0x3F) << 10), delay);
                CcfifoWrite(EMC_PMACRO_VTTGEN_CTRL_1,
                            (pmacro_vttgen_ctrl_1 & 0xFFFF03FF) | ((to_table->vtt_vdda_ctrl_0 & 0x3F) << 10), delay * 2);
            } else {
                CcfifoWrite(EMC_PMACRO_VTTGEN_CTRL_1,
                            (pmacro_vttgen_ctrl_1 & 0xFFFF03FF) | ((dst_timing->vtt_vdda_ctrl_0 & 0x3F) << 10), 0);
            }
        }
    } else {
        uint32_t pmacro_vttgen_ctrl_1 = RegRead(EMC + EMC_PMACRO_VTTGEN_CTRL_1);

        if (dst_timing->vtt_vdda_dual_channel) {
            if (vtt_vdda_channel == 1) {
                CcfifoWrite(EMC_PMACRO_VTTGEN_CTRL_1,
                            (pmacro_vttgen_ctrl_1 & 0xFFFF03FF) | ((dst_timing->vtt_vdda_ctrl_3 & 0x3F) << 10), delay);
                CcfifoWrite(EMC_PMACRO_VTTGEN_CTRL_1,
                            (pmacro_vttgen_ctrl_1 & 0xFFFF03FF) | ((dst_timing->vtt_vdda_ctrl_0 & 0x3F) << 10), delay);
            } else if (vtt_vdda_channel == 0) {
                CcfifoWrite(EMC_PMACRO_VTTGEN_CTRL_1,
                            (pmacro_vttgen_ctrl_1 & 0xFFFF03FF) | ((dst_timing->vtt_vdda_ctrl_1 & 0x3F) << 10), delay);
                CcfifoWrite(EMC_PMACRO_VTTGEN_CTRL_1,
                            (pmacro_vttgen_ctrl_1 & 0xFFFF03FF) | ((dst_timing->vtt_vdda_ctrl_2 & 0x3F) << 10), delay);
            }
        } else {
            CcfifoWrite(EMC_PMACRO_VTTGEN_CTRL_1,
                        (pmacro_vttgen_ctrl_1 & 0xFFFF03FF) | ((dst_timing->vtt_vdda_ctrl_0 & 0x3F) << 10), 0);
        }
    }
}

static uint32_t DvfsPowerRampUp(uint32_t dst_clock_period, bool flip_backward,
                                  EmcDvfsTimingTable *src_timing,
                                  EmcDvfsTimingTable *dst_timing,
                                  uint32_t training) {
    uint32_t misc_cfg_1 = flip_backward ? src_timing->misc_cfg_1 : dst_timing->misc_cfg_1;

    uint32_t emc_pmacro_cmd_pad_tx_ctrl, emc_pmacro_brick_ctrl_rfu1, emc_fbio_cfg5;

    if (flip_backward) {
        emc_pmacro_cmd_pad_tx_ctrl = src_timing->burst_regs.emc_pmacro_cmd_pad_tx_ctrl;
        emc_pmacro_brick_ctrl_rfu1 = src_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1;
        emc_fbio_cfg5              = src_timing->burst_regs.emc_fbio_cfg5;
    } else if (training & (CA_TRAINING | CA_VREF_TRAINING)) {
        emc_pmacro_cmd_pad_tx_ctrl = dst_timing->shadow_regs_ca_train.emc_pmacro_cmd_pad_tx_ctrl;
        emc_pmacro_brick_ctrl_rfu1 = dst_timing->shadow_regs_ca_train.emc_pmacro_brick_ctrl_rfu1;
        emc_fbio_cfg5              = dst_timing->shadow_regs_ca_train.emc_fbio_cfg5;
    } else if (training & (WRITE_TRAINING | WRITE_VREF_TRAINING | READ_TRAINING | READ_VREF_TRAINING)) {
        emc_pmacro_cmd_pad_tx_ctrl = dst_timing->shadow_regs_rdwr_train.emc_pmacro_cmd_pad_tx_ctrl;
        emc_pmacro_brick_ctrl_rfu1 = dst_timing->shadow_regs_rdwr_train.emc_pmacro_brick_ctrl_rfu1;
        emc_fbio_cfg5              = dst_timing->shadow_regs_rdwr_train.emc_fbio_cfg5;
    } else {
        emc_pmacro_cmd_pad_tx_ctrl = dst_timing->burst_regs.emc_pmacro_cmd_pad_tx_ctrl;
        emc_pmacro_brick_ctrl_rfu1 = dst_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1;
        emc_fbio_cfg5              = dst_timing->burst_regs.emc_fbio_cfg5;
    }

    bool     misc_flag = (misc_cfg_1 & 3) == 3;
    uint32_t timescale = 100000u << ((misc_cfg_1 >> 2) & 7);
    uint32_t delay     = timescale / dst_clock_period;

    if (dst_clock_period < 869 || misc_flag) {
        CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, emc_pmacro_brick_ctrl_rfu1 & 0xFE40FE40, delay + 1);
        CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, emc_pmacro_brick_ctrl_rfu1 & 0xFEEDFEED, delay + 1);
        CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, emc_pmacro_brick_ctrl_rfu1 & 0xFFFFFFFF, delay + 1);
        CcfifoWrite(EMC_FBIO_CFG5,              emc_fbio_cfg5 & 0xFFFFFEFF, delay + 10);
        CcfifoWrite(EMC_PMACRO_CMD_PAD_TX_CTRL, emc_pmacro_cmd_pad_tx_ctrl & 0xFBFFFFFF, 5);
        return timescale + 10 * dst_clock_period + 3 * timescale;
    } else if (dst_clock_period > 1665) {
        CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, emc_pmacro_brick_ctrl_rfu1 | 0x00000600, 0);
        CcfifoWrite(EMC_FBIO_CFG5,              emc_fbio_cfg5 & 0xFFFFFEFF, 12);
        CcfifoWrite(EMC_PMACRO_CMD_PAD_TX_CTRL, emc_pmacro_cmd_pad_tx_ctrl & 0xFBFFFFFF, 5);
        return 12 * dst_clock_period;
    } else {
        CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, emc_pmacro_brick_ctrl_rfu1 | 0x06000600, delay + 1);
        CcfifoWrite(EMC_FBIO_CFG5,              emc_fbio_cfg5 & 0xFFFFFEFF, delay + 10);
        CcfifoWrite(EMC_PMACRO_CMD_PAD_TX_CTRL, emc_pmacro_cmd_pad_tx_ctrl & 0xFBFFFFFF, 5);
        return timescale + 10 * dst_clock_period;
    }
}

static void FreqChange(EmcDvfsTimingTable *src_timing, EmcDvfsTimingTable *dst_timing,
                        uint32_t training, uint32_t dst_clk_src) {
    const bool train_ca          = (training & CA_TRAINING)         != 0;
    const bool train_ca_vref     = (training & CA_VREF_TRAINING)    != 0;
    const bool train_wr          = (training & WRITE_TRAINING)      != 0;
    const bool train_wr_vref     = (training & WRITE_VREF_TRAINING) != 0;
    const bool train_rd          = (training & READ_TRAINING)       != 0;
    const bool train_rd_vref     = (training & READ_VREF_TRAINING)  != 0;
    const bool train_second_rank = (training & TRAIN_SECOND_RANK)   != 0;
    const bool train_bit_level   = (training & BIT_LEVEL_TRAINING)  != 0;

    const bool training_enabled = (training & (CA_TRAINING | CA_VREF_TRAINING |
                                               WRITE_TRAINING | WRITE_VREF_TRAINING |
                                               READ_TRAINING  | READ_VREF_TRAINING)) != 0;

    uint32_t dst_emc_fbio_cfg7 = dst_timing->emc_fbio_cfg7;
    uint32_t dst_misc_cfg_0    = dst_timing->misc_cfg_0;
    uint32_t dst_misc_cfg_1    = dst_timing->misc_cfg_1;
    uint32_t dst_misc_cfg_2    = dst_timing->misc_cfg_2;
    uint32_t src_misc_cfg_0    = src_timing->misc_cfg_0;
    uint32_t src_misc_cfg_1    = src_timing->misc_cfg_1;
    uint32_t src_t_rp          = src_timing->dram_timings.t_rp;
    uint32_t src_t_rfc         = src_timing->dram_timings.t_rfc;
    uint32_t dst_t_pdex        = dst_timing->dram_timings.t_pdex;
    uint32_t dst_t_fc_lpddr4   = dst_timing->dram_timings.t_fc_lpddr4;

    const bool ch0_enable = RegGetField(dst_emc_fbio_cfg7, EMC_REG_BITS_MASK(FBIO_CFG7_CH0_ENABLE)) == EMC_FBIO_CFG7_CH0_ENABLE_ENABLE;
    const bool ch1_enable = RegGetField(dst_emc_fbio_cfg7, EMC_REG_BITS_MASK(FBIO_CFG7_CH1_ENABLE)) == EMC_FBIO_CFG7_CH1_ENABLE_ENABLE;

    g_fsp_for_next_freq = !g_fsp_for_next_freq;

    const int dram_type = (int)RegGetValue(EMC + EMC_FBIO_CFG5, EMC_REG_BITS_MASK(FBIO_CFG5_DRAM_TYPE));
    uint32_t src_emc_zcal_wait_cnt = src_timing->burst_regs.emc_zcal_wait_cnt;

    bool shared_zq_resistor = (src_emc_zcal_wait_cnt >> 31) & 1;
    bool opt_zcal_en_cc     = (dst_timing->burst_regs.emc_zcal_interval && !src_timing->burst_regs.emc_zcal_interval)
                               || (dram_type == DRAM_TYPE_LPDDR4);

    uint32_t dst_t_fc_lpddr4_hz = 1000 * dst_t_fc_lpddr4;

    bool is_lpddr2 = (dram_type == DRAM_TYPE_LPDDR2);
    bool is_lpddr3 = is_lpddr2 && ((dst_timing->burst_regs.emc_fbio_cfg5 >> 25) & 1);
    uint32_t opt_dll_mode = (dram_type == DRAM_TYPE_DDR4) ? GetDllState(dst_timing) : DLL_OFF;

    int dram_dev_num = (int)((RegRead(MC + MC_EMEM_ADR_CFG) & 1) + 1);

    uint32_t tZQCAL_lpddr4           = dst_timing->tZQCAL_lpddr4;
    uint32_t zqcal_before_cc_cutoff  = dst_timing->zqcal_before_cc_cutoff;
    uint32_t opt_cc_short_zcal       = dst_timing->opt_cc_short_zcal;
    uint32_t opt_short_zcal          = dst_timing->opt_short_zcal;
    uint32_t opt_do_sw_qrst          = dst_timing->opt_do_sw_qrst;
    uint32_t save_restore_clkstop_pd = dst_timing->save_restore_clkstop_pd;
    uint32_t cya_allow_ref_cc        = dst_timing->cya_allow_ref_cc;
    uint32_t ref_b4_sref_en          = dst_timing->ref_b4_sref_en;
    uint32_t cya_issue_pc_ref        = dst_timing->cya_issue_pc_ref;

    uint32_t src_rate_khz = src_timing->rate_khz;
    uint32_t dst_rate_khz = dst_timing->rate_khz;

    uint32_t src_clock_period = 1000000000u / src_rate_khz;
    uint32_t dst_clock_period = 1000000000u / dst_rate_khz;

    uint32_t emc_auto_cal_config = RegRead(EMC + EMC_AUTO_CAL_CONFIG);

    uint32_t adj_dst_t_fc_lpddr4 = (dst_clock_period <= zqcal_before_cc_cutoff) ? dst_t_fc_lpddr4_hz : 0;

    uint32_t emc_dbg_o          = RegRead(EMC + EMC_DBG);
    uint32_t emc_pin_o          = RegRead(EMC + EMC_PIN);
    uint32_t emc_cfg_pipe_clk_o = RegRead(EMC + EMC_CFG_PIPE_CLK);
    uint32_t emc_dbg            = emc_dbg_o;

    uint32_t emc_cfg          = dst_timing->burst_regs.emc_cfg;
    uint32_t emc_sel_dpd_ctrl = dst_timing->emc_sel_dpd_ctrl;

    uint32_t next_push, next_dq_e_ivref, next_dqs_e_ivref;

    /* Step 1 */
    {
        uint32_t tmp = RegRead(EMC + EMC_CFG_DIG_DLL);
        tmp &= ~(1u << 0);
        RegWrite(EMC + EMC_CFG_DIG_DLL, tmp);
    }

    uint32_t div_14000_by_dst_period = u32_max(UtilDivideUp(14000, dst_clock_period), 10);

    TimingUpdate(dst_emc_fbio_cfg7);
    WaitForUpdate(EMC_CFG_DIG_DLL, (1 << 0), false, dst_emc_fbio_cfg7);

    emc_auto_cal_config = (dst_timing->emc_auto_cal_config & 0x7FFFF9FF) | 0x600;
    RegWrite(EMC + EMC_AUTO_CAL_CONFIG, emc_auto_cal_config);
    RegRead(EMC + EMC_AUTO_CAL_CONFIG);

    emc_dbg = SetShadowBypass(ACTIVE, emc_dbg_o);
    RegWrite(EMC + EMC_DBG, emc_dbg);
    RegWrite(EMC + EMC_CFG,          emc_cfg & 0x0FFFFFFF);
    RegWrite(EMC + EMC_SEL_DPD_CTRL, emc_sel_dpd_ctrl & 0xFFFFFEC3);
    RegWrite(EMC + EMC_DBG, emc_dbg_o);

    bool     compensate_trimmer_applicable = false;
    uint32_t adel = 0;
    if (!training_enabled && dst_timing->periodic_training) {
        WaitForUpdate(EMC_EMC_STATUS, (uint32_t)(dram_dev_num == TWO_RANK ? 0x30 : 0x10), false, dst_emc_fbio_cfg7);
        WaitForUpdate(EMC_EMC_STATUS, 0x300, false, dst_emc_fbio_cfg7);

        if (dst_timing->periodic_training) {
            dst_timing->current_dram_clktree_c0d0u0 = dst_timing->trained_dram_clktree_c0d0u0;
            dst_timing->current_dram_clktree_c0d0u1 = dst_timing->trained_dram_clktree_c0d0u1;
            dst_timing->current_dram_clktree_c0d1u0 = dst_timing->trained_dram_clktree_c0d1u0;
            dst_timing->current_dram_clktree_c0d1u1 = dst_timing->trained_dram_clktree_c0d1u1;
            dst_timing->current_dram_clktree_c1d0u0 = dst_timing->trained_dram_clktree_c1d0u0;
            dst_timing->current_dram_clktree_c1d0u1 = dst_timing->trained_dram_clktree_c1d0u1;
            dst_timing->current_dram_clktree_c1d1u0 = dst_timing->trained_dram_clktree_c1d1u0;
            dst_timing->current_dram_clktree_c1d1u1 = dst_timing->trained_dram_clktree_c1d1u1;

            adel = PeriodicCompensationHandler(DVFS_SEQUENCE, (uint32_t)dram_dev_num, src_timing, dst_timing);
            compensate_trimmer_applicable = dst_timing->periodic_training &&
                                            ((adel * 128 * (dst_rate_khz / 1000)) / 1000000) > dst_timing->tree_margin;
        }
    }

    RegWrite(EMC + EMC_INTSTATUS, (1u << 4));

    emc_dbg = SetShadowBypass(ACTIVE, emc_dbg);
    RegWrite(EMC + EMC_DBG, emc_dbg);
    RegWrite(EMC + EMC_CFG,               emc_cfg & 0x0FFFFFFF);
    RegWrite(EMC + EMC_SEL_DPD_CTRL,      emc_sel_dpd_ctrl & 0xFFFFFEC3);
    RegWrite(EMC + EMC_CFG_PIPE_CLK,      emc_cfg_pipe_clk_o | (1 << 0));
    RegWrite(EMC + EMC_FDPD_CTRL_CMD_NO_RAMP, dst_timing->emc_fdpd_ctrl_cmd_no_ramp & ~(1u << 0));

    if (dst_timing->pllm_misc1_0_pllm_clamp_ph90) {
        RegClearBits(CLKRST + CLK_RST_CONTROLLER_PLLM_MISC1, 0x80000000);
    }

    if (((!(src_timing->burst_regs.emc_pmacro_data_pad_tx_ctrl & (1 <<  0))) &&
          (dst_timing->burst_regs.emc_pmacro_data_pad_tx_ctrl  & (1 <<  0)))  ||
        ((!(src_timing->burst_regs.emc_pmacro_data_pad_tx_ctrl & (1 << 10))) &&
          (dst_timing->burst_regs.emc_pmacro_data_pad_tx_ctrl  & (1 << 10))))
    {
        uint32_t pad_tx_ctrl      = dst_timing->burst_regs.emc_pmacro_data_pad_tx_ctrl;
        uint32_t last_pad_tx_ctrl = src_timing->burst_regs.emc_pmacro_data_pad_tx_ctrl;

        next_dqs_e_ivref = pad_tx_ctrl & (1 << 10);
        next_dq_e_ivref  = pad_tx_ctrl & (1 <<  0);
        next_push = (last_pad_tx_ctrl & ~(1u << 0) & ~(1u << 10)) | next_dq_e_ivref | next_dqs_e_ivref;
        RegWrite(EMC + EMC_PMACRO_DATA_PAD_TX_CTRL, next_push);
        RegWrite(EMC + EMC_DBG, emc_dbg_o);
        usleep(1);
    } else {
        RegWrite(EMC + EMC_DBG, emc_dbg_o);
    }

    if ((dst_misc_cfg_1 & 0x20) == 0) {
        RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
        uint32_t xm2comppadctrl = RegRead(EMC + EMC_XM2COMPPADCTRL);
        RegWrite(EMC + EMC_XM2COMPPADCTRL, xm2comppadctrl | 0x08000000); usleep(1);
        RegWrite(EMC + EMC_XM2COMPPADCTRL, xm2comppadctrl | 0x18000000); usleep(1);
        RegWrite(EMC + EMC_XM2COMPPADCTRL, xm2comppadctrl | 0x38000000); usleep(1);
        RegWrite(EMC + EMC_DBG, SetShadowBypass(ASSEMBLY, emc_dbg_o));
    }

    /* Step 2 */
    if (dst_timing->burst_regs.emc_cfg_dig_dll & (1 << 0)) {
        RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
        RegClearBits(EMC + EMC_PMACRO_DLL_CFG_1, 0x2000);
        RegRead(EMC + EMC_PMACRO_DLL_CFG_1);
        RegWrite(EMC + EMC_DBG, SetShadowBypass(ASSEMBLY, emc_dbg_o));
        RegRead(EMC + EMC_DBG);
        DllPrelock(dst_timing, src_timing, training_enabled, dst_clk_src);
    } else {
        DllDisable(dst_emc_fbio_cfg7);
    }

    /* Step 3 */
    emc_auto_cal_config = (dst_timing->emc_auto_cal_config & 0x7FFFF9FF) | 0x600;
    RegWrite(EMC + EMC_AUTO_CAL_CONFIG, emc_auto_cal_config);

    RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
    RegWrite(EMC + EMC_AUTO_CAL_CONFIG2, dst_timing->emc_auto_cal_config2);

    if (ch0_enable || ch1_enable) {
        RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG3, dst_timing->emc_auto_cal_config3);
        RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG4, dst_timing->emc_auto_cal_config4);
        RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG5, dst_timing->emc_auto_cal_config5);
        RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG6, dst_timing->emc_auto_cal_config6);
        RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG7, dst_timing->emc_auto_cal_config7);
        RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG8, dst_timing->emc_auto_cal_config8);
    }

    RegWrite(EMC + EMC_DBG, emc_dbg_o);
    emc_auto_cal_config = (dst_timing->emc_auto_cal_config & 0x7FFFF9FE) | 0x601;
    RegWrite(EMC + EMC_AUTO_CAL_CONFIG, emc_auto_cal_config);

    /* Step 4 */
    RegClearBits(EMC + EMC_CFG, 0x10000000);
    RegWrite(EMC + EMC_CFG_2, dst_timing->emc_cfg_2);

    /* Step 5 */
    uint32_t emc_zcal_interval    = src_timing->burst_regs.emc_zcal_interval & 0xFF000000;
    uint32_t dst_emc_zcal_wait_cnt = dst_timing->burst_regs.emc_zcal_wait_cnt;
    uint32_t zq_wait_long, zq_wait_short;

    if (dram_type == DRAM_TYPE_LPDDR4) {
        zq_wait_long  = u32_max(UtilDivideUp(1000000, dst_clock_period), 1);
        zq_wait_short = u32_max(UtilDivideUp(30000,   dst_clock_period), 8) + 1;
    } else if (is_lpddr2 || is_lpddr3) {
        zq_wait_long  = u32_max(UtilDivideUp(360000, dst_clock_period), dst_timing->min_mrs_wait) + 4;
        zq_wait_short = 0;
    } else if (dram_type == DRAM_TYPE_DDR4) {
        zq_wait_long  = u32_max(UtilDivideUp(320000, dst_clock_period), 256);
        zq_wait_short = 0;
    } else {
        zq_wait_long  = 0;
        zq_wait_short = 0;
    }

    /* Step 6 */
    {
        uint32_t pintemp = RegRead(EMC + EMC_PIN);
        if ((train_ca || train_ca_vref) && (dram_dev_num == TWO_RANK)) {
            RegWrite(EMC + EMC_PIN, pintemp | 0x7);
        }
    }

    /* Step 7 */
    uint32_t mr13_flip_fspop, mr13_flip_fspwr;
    if (!g_fsp_for_next_freq) {
        mr13_flip_fspwr = (dst_timing->emc_mrw3 & 0xffffff3f) | 0x80;
        mr13_flip_fspop = (dst_timing->emc_mrw3 & 0xffffff3f) | 0x00;
    } else {
        mr13_flip_fspwr = (dst_timing->emc_mrw3 & 0xffffff3f) | 0x40;
        mr13_flip_fspop = (dst_timing->emc_mrw3 & 0xffffff3f) | 0xc0;
    }

    uint32_t mr13_catr_enable = mr13_flip_fspwr | 1;

    if (dram_dev_num == TWO_RANK) {
        if (train_ca || train_ca_vref)
            mr13_flip_fspop = (mr13_flip_fspop & 0x3FFFFFFF) | (train_second_rank ? 0x80000000 : 0x40000000);
        mr13_catr_enable = (mr13_catr_enable & 0x3FFFFFFF) | (train_second_rank ? 0x40000000 : 0x80000000);
    }

    if (dram_type == DRAM_TYPE_LPDDR4) {
        RegWrite(EMC + EMC_MRW3, mr13_flip_fspwr);
        RegWrite(EMC + EMC_MRW,  dst_timing->emc_mrw);
        RegWrite(EMC + EMC_MRW2, dst_timing->emc_mrw2);
    }

    /* Step 8: shadow registers */
    {
        uint32_t pmacro_vttgen_ctrl_1 = RegRead(EMC + EMC_PMACRO_VTTGEN_CTRL_1);
        uint32_t xm2comppadctrl       = RegRead(EMC + EMC_XM2COMPPADCTRL);

        for (uint32_t i = 0; i < dst_timing->num_burst; ++i) {
            if (!BurstRegisters[i]) continue;

            const uint32_t reg_addr = BurstRegisters[i];
            uint32_t wval;

            if (train_ca || train_ca_vref)
                wval = dst_timing->shadow_regs_ca_train_arr[i];
            else if (train_wr || train_wr_vref || train_rd || train_rd_vref)
                wval = dst_timing->shadow_regs_rdwr_train_arr[i];
            else
                wval = dst_timing->burst_regs_arr[i];

            switch (reg_addr) {
                case EMC_BASE + EMC_CFG:
                    wval &= (dram_type == DRAM_TYPE_LPDDR4) ? 0x0FFFFFFF : 0xCFFFFFFF;
                    break;
                case EMC_BASE + EMC_MRS_WAIT_CNT:
                    if (opt_zcal_en_cc && is_lpddr2 && (opt_cc_short_zcal == 0) && (opt_short_zcal != 0))
                        wval = (wval & 0xFFFFFC00) | (zq_wait_long & 0x3FF);
                    break;
                case EMC_BASE + EMC_ZCAL_WAIT_CNT:
                    if ((opt_short_zcal != 0) && opt_zcal_en_cc && (opt_cc_short_zcal == 0) && (dram_type == DRAM_TYPE_DDR4))
                        wval = (wval & 0xFFFFF800) | (zq_wait_long & 0x7FF);
                    break;
                case EMC_BASE + EMC_ZCAL_INTERVAL:
                    if (opt_zcal_en_cc) wval = 0;
                    break;
                case EMC_BASE + EMC_PMACRO_BRICK_CTRL_RFU1:
                    wval &= 0xF800F800;
                    break;
                case EMC_BASE + EMC_PMACRO_CMD_PAD_TX_CTRL:
                    wval |= 0x04000000;
                    break;
                case EMC_BASE + EMC_PMACRO_AUTOCAL_CFG_COMMON:
                    wval |= 0x00010000;
                    break;
                case EMC_BASE + EMC_TRAINING_CTRL:
                    if (train_second_rank) wval |= 0x4000;
                    break;
                case EMC_BASE + EMC_REFRESH:
                case EMC_BASE + EMC_TREFBW:
                    wval >>= 0;
                    break;
                case EMC_BASE + EMC_XM2COMPPADCTRL:
                    if ((dst_misc_cfg_1 & 0x20) == 0)
                        wval = (wval & 0x00FFFFFF) | (xm2comppadctrl & 0xFF000000);
                    break;
                case EMC_BASE + EMC_DLL_CFG_1:
                    wval = (wval & 0xFFFFDFFF) | (RegRead(EMC + EMC_PMACRO_DLL_CFG_1) & 0x00002000);
                    break;
                case EMC_BASE + EMC_PMACRO_VTTGEN_CTRL_1:
                    wval = (wval & 0xFFFF03FF) | (pmacro_vttgen_ctrl_1 & 0xFC00);
                    break;
                case EMC_BASE  + EMC_MRW6:
                case EMC_BASE  + EMC_MRW7:
                case EMC_BASE  + EMC_MRW8:
                case EMC_BASE  + EMC_MRW9:
                case EMC_BASE  + EMC_MRW14:
                case EMC_BASE  + EMC_MRW15:
                case EMC0_BASE + EMC_MRW10:
                case EMC0_BASE + EMC_MRW11:
                case EMC0_BASE + EMC_MRW12:
                case EMC0_BASE + EMC_MRW13:
                case EMC1_BASE + EMC_MRW10:
                case EMC1_BASE + EMC_MRW11:
                case EMC1_BASE + EMC_MRW12:
                case EMC1_BASE + EMC_MRW13:
                    if (dram_type != DRAM_TYPE_LPDDR4) continue;
                    break;
            }

            RegWrite(reg_addr, wval);
        }
    }

    if (dram_type == DRAM_TYPE_LPDDR4) {
        uint32_t mrw_req;
        if (training_enabled)
            mrw_req = (23 << 16) | (src_timing->run_clocks & 0xFF);
        else
            mrw_req = (23 << 16) | (dst_timing->run_clocks & 0xFF);
        RegWrite(EMC + EMC_MRW, mrw_req);
    }

    /* Per-channel burst */
    if (dram_type == DRAM_TYPE_LPDDR4) {
        for (uint32_t i = 0; i < dst_timing->num_burst_per_ch; ++i) {
            if (!PerChannelBurstRegisters[i]) continue;
            const uint32_t addr = PerChannelBurstRegisters[i];
            const uint32_t base = addr & ~0xFFFu;
            if ((!ch0_enable && base == EMC0) || (!ch1_enable && base == EMC1)) continue;
            RegWrite(addr, dst_timing->burst_perch_regs_arr[i]);
        }
    }

    /* Vref regs */
    for (uint32_t i = 0; i < dst_timing->vref_num; ++i) {
        if (!PerChannelVrefRegisters[i]) continue;
        const uint32_t addr = PerChannelVrefRegisters[i];
        const uint32_t base = addr & ~0xFFFu;
        if ((!ch0_enable && base == EMC0) || (!ch1_enable && base == EMC1)) continue;
        RegWrite(addr, dst_timing->vref_perch_regs_arr[i]);
    }

    /* Training regs */
    if (training_enabled) {
        for (uint32_t i = 0; i < dst_timing->training_mod_num; ++i) {
            if (!PerChannelTrainingModRegisters[i]) continue;
            const uint32_t addr = PerChannelTrainingModRegisters[i];
            const uint32_t base = addr & ~0xFFFu;
            if ((!ch0_enable && base == EMC0) || (!ch1_enable && base == EMC1)) continue;
            RegWrite(addr, dst_timing->training_mod_regs_arr[i]);
        }
    }

    /* Per-channel trimmers */
    for (uint32_t i = 0; i < dst_timing->num_trim_per_ch; ++i) {
        if (!PerChannelTrimRegisters[i]) continue;
        const uint32_t addr = PerChannelTrimRegisters[i];
        const uint32_t base = addr & ~0xFFFu;
        if ((!ch0_enable && base == EMC0) || (!ch1_enable && base == EMC1)) continue;

        uint32_t wval = dst_timing->trim_perch_regs_arr[i];
        if (compensate_trimmer_applicable) {
            switch (addr) {
                case EMC0_BASE + EMC_DATA_BRLSHFT_0:
                case EMC1_BASE + EMC_DATA_BRLSHFT_0:
                case EMC0_BASE + EMC_DATA_BRLSHFT_1:
                case EMC1_BASE + EMC_DATA_BRLSHFT_1:
                    wval = ApplyPeriodicCompensationTrimmer(dst_timing, addr);
                    break;
            }
        }
        RegWrite(addr, wval);
    }

    /* Trimmers */
    for (uint32_t i = 0; i < dst_timing->num_trim; ++i) {
        if (!TrimRegisters[i]) continue;
        const uint32_t addr = TrimRegisters[i];
        uint32_t wval = dst_timing->trim_regs_arr[i];

        if (compensate_trimmer_applicable) {
            switch (addr) {
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_2:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_0:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_1:
                case EMC_BASE + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_2:
                    wval = ApplyPeriodicCompensationTrimmer(dst_timing, addr);
                    break;
            }
        }
        RegWrite(addr, wval);
    }

    if (training_enabled) {
        if (train_wr && dst_timing->periodic_training)
            PeriodicCompensationHandler(WRITE_TRAINING_SEQUENCE, (uint32_t)dram_dev_num, src_timing, dst_timing);
    } else {
        for (uint32_t i = 0; i < dst_timing->num_mc_regs; ++i)
            RegWrite(BurstMcRegisters[i], dst_timing->burst_mc_regs_arr[i]);

        if (dst_timing->rate_khz < src_timing->rate_khz) {
            for (uint32_t i = 0; i < dst_timing->num_up_down; ++i)
                RegWrite(LaScaleRegisters[i], dst_timing->la_scale_regs_arr[i]);
        }
    }

    if ((dst_misc_cfg_1 & 2) != 0 && (dst_misc_cfg_1 & 1) == 0) {
        RegWrite(EMC + EMC_PMACRO_BRICK_CTRL_RFU1, dst_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1);
        RegWrite(EMC + EMC_PMACRO_CMD_PAD_TX_CTRL, dst_timing->burst_regs.emc_pmacro_cmd_pad_tx_ctrl);
    }

    RegWrite(EMC + EMC_CFG_PIPE_CLK,          (1 << 0));
    RegWrite(EMC + EMC_FDPD_CTRL_CMD_NO_RAMP,  dst_timing->emc_fdpd_ctrl_cmd_no_ramp & ~(1u << 0));

    /* Step 9 */
    uint32_t emc_dbg_write_active = emc_dbg_o;
    if (dram_type == DRAM_TYPE_LPDDR4) {
        RegWrite(EMC + EMC_ZCAL_INTERVAL,  emc_zcal_interval);
        RegWrite(EMC + EMC_ZCAL_WAIT_CNT, (dst_emc_zcal_wait_cnt & 0xFFFFF800) | 1);
        RegWrite(EMC + EMC_DBG,            emc_dbg_o | ((1 << 1) | (1 << 30)));
        RegWrite(EMC + EMC_ZCAL_INTERVAL,  emc_zcal_interval);
        RegWrite(EMC + EMC_DBG,            emc_dbg_o);

        if (training_enabled) {
            RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
            RegWrite(EMC + EMC_PMACRO_AUTOCAL_CFG_COMMON,
                     dst_timing->burst_regs.emc_pmacro_autocal_cfg_common | (1 << 16));
            if (train_ca || train_ca_vref)
                RegWrite(EMC + EMC_FBIO_CFG5, src_timing->burst_regs.emc_fbio_cfg5 | (1 << 27));
            RegWrite(EMC + EMC_DBG, emc_dbg_o);

            if (dst_emc_fbio_cfg7 == 0x6)
                CcfifoWrite(EMC_CFG_SYNC, 1, 0);

            CcfifoWrite(EMC_DBG, ((emc_dbg_o & 0xF3FFFFFF) | 0x4000000), 0);
        }

        CcfifoWrite(EMC_SELF_REF, 0x101, 0);

        if (!(train_ca || train_ca_vref) && dst_clock_period <= zqcal_before_cc_cutoff) {
            CcfifoWrite(EMC_MRW3,  mr13_flip_fspwr ^ 0x40, 0);
            CcfifoWrite(EMC_MRW6,  (src_timing->burst_regs.emc_mrw6  & 0x0000C0C0) | (dst_timing->burst_regs.emc_mrw6  & 0xFFFF3F3F), 0);
            CcfifoWrite(EMC_MRW14, (src_timing->burst_regs.emc_mrw14 & 0x00003838) | (dst_timing->burst_regs.emc_mrw14 & 0xFFFF0707), 0);
            if (dram_dev_num == TWO_RANK) {
                CcfifoWrite(EMC_MRW7,  (src_timing->burst_regs.emc_mrw7  & 0x0000C0C0) | (dst_timing->burst_regs.emc_mrw7  & 0xFFFF3F3F), 0);
                CcfifoWrite(EMC_MRW15, (src_timing->burst_regs.emc_mrw15 & 0x00003838) | (dst_timing->burst_regs.emc_mrw15 & 0xFFFF0707), 0);
            }
            if (opt_zcal_en_cc) {
                if (dram_dev_num == ONE_RANK || shared_zq_resistor)
                    CcfifoWrite(EMC_ZQ_CAL, (2u << 30) | (1 << 0), 0);
                else
                    CcfifoWrite(EMC_ZQ_CAL, (1 << 0), 0);
            }
        }

        if (training_enabled) {
            emc_dbg_write_active = (emc_dbg_o & 0xF7FFFFFF) | 0x4000000 | (1u << 30);
            CcfifoWrite(EMC_DBG, emc_dbg_write_active, 0);
        }

        if (train_ca || train_ca_vref) {
            CcfifoWrite(EMC_PMACRO_DATA_RX_TERM_MODE,
                        src_timing->burst_regs.emc_pmacro_data_rx_term_mode & 0xFFFFFCCC, 0);

            if (dram_dev_num == TWO_RANK && train_second_rank) {
                CcfifoWrite(EMC_MRW3, mr13_flip_fspop | 0x8, (1000 * src_t_rp) / src_clock_period);
                CcfifoWrite(EMC_MRW3, mr13_catr_enable | 0x8, 0);
            } else {
                CcfifoWrite(EMC_MRW3, mr13_catr_enable | 0x8, (1000 * src_t_rp) / src_clock_period);
            }

            CcfifoWrite(EMC_TR_CTRL_0, (dst_timing->emc_tr_ctrl_0 & 0x3F1000) | 0x100012A, 0);
            CcfifoWrite(EMC_INTSTATUS,  0, 1000000 / src_clock_period);
        } else {
            CcfifoWrite(EMC_MRW3, mr13_flip_fspop | 0x8, (1000 * src_t_rp) / src_clock_period);
            CcfifoWrite(EMC_INTSTATUS, 0,
                u32_max(DivideUpFloat(dst_t_fc_lpddr4_hz, src_clock_period),
                        u32_max(DivideUpFloat(14000, src_clock_period), 10)));
        }

        {
            uint32_t t = 30 + (cya_allow_ref_cc
                ? ((4000 * src_t_rfc + 1000 * src_t_rp) / src_clock_period)
                : 0);
            CcfifoWrite(EMC_PIN, emc_pin_o & 0xFFFFFFF8, t);
        }
    } else {
        CcfifoWrite(EMC_SELF_REF, 0x1, 0);
    }

    uint32_t ref_delay_mult = 1;
    if (ref_b4_sref_en)   ++ref_delay_mult;
    if (cya_allow_ref_cc)  ++ref_delay_mult;
    if (cya_issue_pc_ref)  ++ref_delay_mult;

    uint32_t ref_delay = 20 + ref_delay_mult *
        (((1000 * src_t_rfc) / src_clock_period) + ((1000 * src_t_rp) / src_clock_period));

    /* Step 11 */
    CcfifoWrite(EMC_CFG_SYNC, 1, (dram_type == DRAM_TYPE_LPDDR4) ? 0 : ref_delay);

    bool do_ramp_up = (dst_misc_cfg_1 & 2) == 0 || (dst_misc_cfg_1 & 1) != 0;
    if (!do_ramp_up)
        CcfifoWrite(EMC_FBIO_CFG5, RegRead(EMC + EMC_FBIO_CFG5) & 0xF7FFFFFF, 12);

    CcfifoWrite(EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_write_active & 0xBFFFFFFF), 0);

    if ((dst_misc_cfg_2 & 0x10) == 0)
        DvfsPowerRampDown(src_timing, dst_timing, false, 0);

    CcfifoWrite(EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_write_active | 0x40000000), 0);

    uint32_t ramp_down_wait = 0;
    if (do_ramp_up) {
        CcfifoWrite(EMC_PMACRO_CMD_PAD_TX_CTRL, src_timing->burst_regs.emc_pmacro_cmd_pad_tx_ctrl | (1u << 26), 0);
        CcfifoWrite(EMC_FBIO_CFG5,              src_timing->burst_regs.emc_fbio_cfg5 | 0x100, 12);

        bool misc_flag = (dst_misc_cfg_1 & 3) == 3;
        uint32_t timescale = 100000u << ((dst_misc_cfg_1 >> 2) & 7);
        uint32_t delay     = timescale / src_clock_period;

        if (src_rate_khz > 1150747 || misc_flag) {
            CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, src_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1 & 0xFEEDFEED, delay + 1);
            CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, src_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1 & 0xFE40FE40, delay + 1);
            CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, src_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1 & 0xF800F800, delay + 1);
            ramp_down_wait = 3 * timescale + 12 * src_clock_period;
        } else {
            CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, src_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1 & 0xF800F800, delay + 20);
            ramp_down_wait = timescale + 32 * src_clock_period;
        }

        if (src_rate_khz > 600240 || misc_flag) {
            CcfifoWrite(EMC_INTSTATUS, 0, delay + 1);
            ramp_down_wait += 2 * timescale;
        }
    }

    /* Step 12 */
    CcfifoWrite(EMC_STALL_THEN_EXE_AFTER_CLKCHANGE, 1, 0);
    CcfifoWrite(EMC_INTSTATUS, 0, dst_timing->clkchange_delay);
    CcfifoWrite(EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_write_active & 0xBFFFFFFF), 0);

    if ((dst_misc_cfg_2 & 0x10) == 0)
        DvfsPowerRampDown(src_timing, dst_timing, false, 1);

    if (training_enabled)
        CcfifoWrite(EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_write_active | 0x40000000), 0);

    /* Step 13 */
    uint32_t ramp_up_wait = 0;
    if (do_ramp_up)
        ramp_up_wait = DvfsPowerRampUp(dst_clock_period, false, src_timing, dst_timing, training);

    if (ramp_up_wait < 1001000 && src_timing->ramp_wait != dst_timing->ramp_wait)
        CcfifoWrite(EMC_INTSTATUS, 0, 1 + ((1001000 - ramp_up_wait) / dst_clock_period));

    if (do_ramp_up) {
        CcfifoWrite(EMC_DBG, emc_dbg_write_active, 0);
    } else {
        CcfifoWrite(EMC_FBIO_CFG5, RegRead(EMC + EMC_FBIO_CFG5), 0);
        CcfifoWrite(EMC_DBG,       emc_dbg_write_active, 12);
    }

    /* Step 14 */
    if (dram_type == DRAM_TYPE_LPDDR4) {
        uint32_t pin_val;
        if (train_ca || train_ca_vref) {
            pin_val = (dst_misc_cfg_0 & 1)
                ? ((emc_pin_o & 0xFFFCFFF8) | (((dst_misc_cfg_0 >> 1) & 3) << 16))
                : emc_pin_o;
            pin_val &= 0xFFFFFFF8;
            if (dram_dev_num == TWO_RANK)
                pin_val |= train_second_rank ? 5u : 6u;
        } else {
            pin_val = (dst_misc_cfg_0 & 1)
                ? ((emc_pin_o & 0xFFFCFFFF) | (((dst_misc_cfg_0 >> 1) & 3) << 16))
                : emc_pin_o;
            pin_val &= 0xFFFFFFF8;
            pin_val |= (dram_dev_num == TWO_RANK) ? 7u : 1u;
        }
        CcfifoWrite(EMC_PIN, pin_val, 0);
    }

    /* Step 15 */
    uint32_t dst_t_pdex_hz = 1000 * dst_t_pdex;
    uint32_t zq_latch_dvfs_wait_time;
    if (dst_clock_period <= zqcal_before_cc_cutoff)
        zq_latch_dvfs_wait_time = (ramp_up_wait + ramp_down_wait) / dst_clock_period;
    else
        zq_latch_dvfs_wait_time = UtilDivideUp(dst_t_pdex_hz, dst_clock_period);

    if (!(train_ca || train_ca_vref) && (dram_type == DRAM_TYPE_LPDDR4) && opt_zcal_en_cc) {
        int offset    = (int)((tZQCAL_lpddr4 - adj_dst_t_fc_lpddr4) / dst_clock_period) - (int)zq_latch_dvfs_wait_time;
        int addl_wait = (int)UtilDivideUp(dst_t_pdex_hz, dst_clock_period);

        if (dram_dev_num == TWO_RANK) {
            if (shared_zq_resistor) {
                if (dst_clock_period > zqcal_before_cc_cutoff)
                    CcfifoWrite(EMC_ZQ_CAL, (2u << 30) | (1 << 0), (uint32_t)int_max(0, addl_wait));
                CcfifoWrite(EMC_ZQ_CAL, (2u << 30) | (1 << 1), (uint32_t)int_max(0, offset + addl_wait));
                CcfifoWrite(EMC_ZQ_CAL, (1u << 30) | (1 << 0), 0);
                if (!training_enabled) {
                    CcfifoWrite(EMC_MRW3,    (mr13_flip_fspop & 0xFFFFFFF7) | 0xC000000 | (dst_misc_cfg_2 & 8), 0);
                    CcfifoWrite(EMC_SELF_REF, 0, 0);
                    CcfifoWrite(EMC_REF,      0, 0);
                }
                CcfifoWrite(EMC_ZQ_CAL, (1u << 30) | (1 << 1),
                            zq_wait_short + div_14000_by_dst_period + (tZQCAL_lpddr4 / dst_clock_period));
            } else {
                if (dst_clock_period > zqcal_before_cc_cutoff)
                    CcfifoWrite(EMC_ZQ_CAL, (0u << 30) | (1 << 0), (uint32_t)int_max(0, addl_wait));
                if (!training_enabled) {
                    CcfifoWrite(EMC_MRW3,    (mr13_flip_fspop & 0xFFFFFFF7) | 0xC000000 | (dst_misc_cfg_2 & 8), (uint32_t)int_max(0, addl_wait));
                    CcfifoWrite(EMC_SELF_REF, 0, 0);
                    CcfifoWrite(EMC_REF,      0, 0);
                }
                CcfifoWrite(EMC_ZQ_CAL, (0u << 30) | (1 << 1),
                            div_14000_by_dst_period + (uint32_t)int_max(0, offset));
            }
        } else {
            if (dst_clock_period > zqcal_before_cc_cutoff)
                CcfifoWrite(EMC_ZQ_CAL, (2u << 30) | (1 << 0), (uint32_t)int_max(0, addl_wait));
            if (!training_enabled) {
                CcfifoWrite(EMC_MRW3,    (mr13_flip_fspop & 0xFFFFFFF7) | 0xC000000 | (dst_misc_cfg_2 & 8), (uint32_t)int_max(0, addl_wait));
                CcfifoWrite(EMC_SELF_REF, 0, 0);
                CcfifoWrite(EMC_REF,      0x80000000, 0);
            }
            CcfifoWrite(EMC_ZQ_CAL, (2u << 30) | (1 << 1),
                        div_14000_by_dst_period + (uint32_t)int_max(0, offset));
        }
    }

    CcfifoWrite(EMC_INTSTATUS, 0, 10);

    /* Step 16 */
    if (training_enabled && dram_type == DRAM_TYPE_LPDDR4) {
        if (opt_do_sw_qrst) {
            CcfifoWrite(EMC_ISSUE_QRST, 1, 0);
            CcfifoWrite(EMC_ISSUE_QRST, 0, 2);
        }

        CcfifoWrite(EMC_INTSTATUS, 0, 1020000 / dst_clock_period);

        {
            uint32_t train_cmd = 0;
            if (train_ca)       train_cmd |= (1 << 1);
            if (train_ca_vref)  train_cmd |= (1 << 5);
            if (train_wr)       train_cmd |= (1 << 3);
            if (train_wr_vref)  train_cmd |= (1 << 6);
            if (train_rd)       train_cmd |= (1 << 2);
            if (train_rd_vref)  train_cmd |= (1 << 7);
            train_cmd |= (1u << 31);
            CcfifoWrite(EMC_TRAINING_CMD, train_cmd, 0);
        }

        CcfifoWrite(EMC_SWITCH_BACK_CTRL, 1, 0);

        if (!(train_ca || train_ca_vref) || train_second_rank) {
            CcfifoWrite(EMC_MRW3,      mr13_flip_fspop ^ 0xC0, 0);
            CcfifoWrite(EMC_INTSTATUS, 0, 1000000 / dst_clock_period);
        }

        {
            uint32_t pin_val = (dst_misc_cfg_0 & 1)
                ? ((emc_pin_o & 0xFFFCFFFF) | (((dst_misc_cfg_0 >> 1) & 3) << 16))
                : emc_pin_o;
            CcfifoWrite(EMC_PIN, pin_val & 0xFFFFFFF8, 0);
        }

        CcfifoWrite(EMC_CFG_SYNC, 1, 0);

        if ((src_misc_cfg_1 & 3) != 2) {
            CcfifoWrite(EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_write_active | 0x40000000), 0);
            CcfifoWrite(EMC_PMACRO_CMD_PAD_TX_CTRL,
                        dst_timing->burst_regs.emc_pmacro_cmd_pad_tx_ctrl | (1u << 26), 0);
            CcfifoWrite(EMC_FBIO_CFG5, dst_timing->burst_regs.emc_fbio_cfg5 | 0x100, 12);

            bool misc_flag2 = (src_misc_cfg_1 & 3) == 3;
            uint32_t timescale2 = 100000u << ((src_misc_cfg_1 >> 2) & 7);
            uint32_t delay2     = timescale2 / dst_clock_period;

            if (dst_rate_khz > 1150747 || misc_flag2) {
                CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, dst_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1 & 0xFEEDFEED, delay2 + 1);
                CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, dst_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1 & 0xFE40FE40, delay2 + 1);
                CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, dst_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1 & 0xF800F800, delay2 + 1);
            } else {
                CcfifoWrite(EMC_PMACRO_BRICK_CTRL_RFU1, dst_timing->burst_regs.emc_pmacro_brick_ctrl_rfu1 & 0xF800F800, delay2 + 20);
            }

            if (dst_rate_khz > 600240 || misc_flag2)
                CcfifoWrite(EMC_INTSTATUS, 0, delay2 + 1);
        } else {
            CcfifoWrite(EMC_FBIO_CFG5, RegRead(EMC + EMC_FBIO_CFG5) & 0xF7FFFFFF, 12);
            CcfifoWrite(EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_write_active | 0x40000000), 0);
        }

        CcfifoWrite(EMC_STALL_THEN_EXE_AFTER_CLKCHANGE, 1, 0);
        CcfifoWrite(EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_write_active & 0xBFFFFFFF), 0);

        if ((dst_misc_cfg_2 & 0x10) == 0)
            DvfsPowerRampDown(src_timing, dst_timing, true, 1);

        if ((src_misc_cfg_1 & 3) != 2) {
            DvfsPowerRampUp(src_clock_period, true, src_timing, dst_timing, training);

            if (ramp_up_wait < 1001000 && src_timing->ramp_wait != dst_timing->ramp_wait)
                CcfifoWrite(EMC_INTSTATUS, 0, 1 + ((1001000 - ramp_up_wait) / dst_clock_period));

            CcfifoWrite(EMC_DBG, emc_dbg_write_active, 0);
        } else {
            if (ramp_up_wait < 1001000 && src_timing->ramp_wait != dst_timing->ramp_wait)
                CcfifoWrite(EMC_INTSTATUS, 0, 1 + ((1001000 - ramp_up_wait) / dst_clock_period));

            CcfifoWrite(EMC_FBIO_CFG5, RegRead(EMC + EMC_FBIO_CFG5), 0);
            CcfifoWrite(EMC_DBG,       emc_dbg_write_active, 12);
        }

        {
            uint32_t pin_val = (src_misc_cfg_0 & 1)
                ? ((emc_pin_o & 0xFFFCFFFF) | (((src_misc_cfg_0 >> 1) & 3) << 16))
                : emc_pin_o;
            pin_val &= 0xFFFFFFF8;
            pin_val |= (dram_dev_num == TWO_RANK) ? 7u : 1u;
            CcfifoWrite(EMC_PIN, pin_val, 0);
        }

        if (train_ca || train_ca_vref) {
            CcfifoWrite(EMC_TR_CTRL_0, 0x2A, 200000 / src_clock_period);
            CcfifoWrite(EMC_TR_CTRL_0, 0x20, 1000000 / src_clock_period);
            CcfifoWrite(EMC_MRW3,      mr13_catr_enable & 0xFFFFFFFE, 0);
            CcfifoWrite(EMC_INTSTATUS, 0, 1000000 / src_clock_period);
            CcfifoWrite(EMC_PMACRO_DATA_RX_TERM_MODE, src_timing->burst_regs.emc_pmacro_data_rx_term_mode, 0);
        }

        CcfifoWrite(EMC_DBG, emc_dbg_o, 0);

        if (opt_zcal_en_cc) {
            if (shared_zq_resistor) {
                CcfifoWrite(EMC_ZQ_CAL, (2u << 30) | (1 << 0), 0);
                CcfifoWrite(EMC_ZQ_CAL, (2u << 30) | (1 << 1),
                            1 + UtilDivideUp(1000000, src_clock_period));
                if ((!(train_ca || train_ca_vref) || train_second_rank) && dram_dev_num == TWO_RANK) {
                    CcfifoWrite(EMC_ZQ_CAL, (1u << 30) | (1 << 0), 0);
                    CcfifoWrite(EMC_ZQ_CAL, (1u << 30) | (1 << 1),
                                1 + UtilDivideUp(1000000, src_clock_period));
                }
            } else {
                CcfifoWrite(EMC_ZQ_CAL,
                            ((dram_dev_num == ONE_RANK ? 2u : 0u) << 30) | (1 << 0), 0);
                CcfifoWrite(EMC_ZQ_CAL,
                            ((dram_dev_num == ONE_RANK ? 2u : 0u) << 30) | (1 << 1),
                            1 + UtilDivideUp(1000000, src_clock_period));
            }
        }

        if (!(train_ca || train_ca_vref))
            CcfifoWrite(EMC_MRW3,
                        ((mr13_flip_fspop & 0xF3FFFFF7) | (dst_misc_cfg_2 & 8)) ^ 0x0C0000C0, 0);

        CcfifoWrite(EMC_SELF_REF, 0x0, 0);
    }

    /* Step 17 */
    if (dram_type != DRAM_TYPE_LPDDR4)
        CcfifoWrite(EMC_SELF_REF, 0, 0);

    /* Step 18 */
    if (is_lpddr2) {
        CcfifoWrite(EMC_MRW2, dst_timing->emc_mrw2, 0);
        CcfifoWrite(EMC_MRW,  dst_timing->emc_mrw,  0);
        if (is_lpddr3)
            CcfifoWrite(EMC_MRW4, dst_timing->emc_mrw4, 0);
    } else if (dram_type == DRAM_TYPE_DDR4) {
        if (opt_dll_mode)
            CcfifoWrite(EMC_EMRS, dst_timing->emc_emrs & ~(1u << 26), 0);
        CcfifoWrite(EMC_EMRS2, dst_timing->emc_emrs2 & ~(1u << 26), 0);
        CcfifoWrite(EMC_MRS,   dst_timing->emc_mrs   |  (1u << 26), 0);
    }

    /* Step 19 */
    if (opt_zcal_en_cc) {
        if (is_lpddr2) {
            uint32_t zq_op = opt_cc_short_zcal ? dst_timing->zq_op_cc_short_zcal : dst_timing->zq_op_cc_long_zcal;
            uint32_t zcal_wait_time_ps = opt_cc_short_zcal
                ? dst_timing->zcal_wait_time_ps_cc_short_zcal
                : dst_timing->zcal_wait_time_ps_cc_long_zcal;
            uint32_t zcal_wait_time_clocks = UtilDivideUp(zcal_wait_time_ps, dst_clock_period);

            CcfifoWrite(EMC_MRS_WAIT_CNT2,
                        (zcal_wait_time_clocks & 0x3FF) | ((zcal_wait_time_clocks & 0x7FF) << 16), 0);
            CcfifoWrite(EMC_MRW, (zq_op | 0x880C0000) - 0x20000, 0);
            if (dram_dev_num == TWO_RANK)
                CcfifoWrite(EMC_MRW, zq_op | 0x480A0000, 0);
        } else if (dram_type == DRAM_TYPE_DDR4) {
            CcfifoWrite(EMC_ZQ_CAL, (2u << 30) | (1 << 0) | (opt_cc_short_zcal ? 0 : (1 << 4)), 0);
            if (dram_dev_num == TWO_RANK)
                CcfifoWrite(EMC_ZQ_CAL, (1u << 30) | (1 << 0) | (opt_cc_short_zcal ? 0 : (1 << 4)), 0);
        }
    }

    /* Step 20 */
    if (training_enabled || dram_type != DRAM_TYPE_LPDDR4)
        CcfifoWrite(EMC_REF, (dram_dev_num == ONE_RANK) ? 0x80000000 : 0x00000000, 0);

    if (opt_do_sw_qrst) {
        CcfifoWrite(EMC_ISSUE_QRST, 1, 0);
        CcfifoWrite(EMC_ISSUE_QRST, 0, 2);
    }

    /* Step 21 */
    if (save_restore_clkstop_pd || opt_zcal_en_cc ||
        (training_enabled && dram_type == DRAM_TYPE_LPDDR4))
    {
        CcfifoWrite(EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o), 0);

        if (opt_zcal_en_cc) {
            if (training_enabled)
                CcfifoWrite(EMC_ZCAL_INTERVAL,
                            (dst_misc_cfg_2 & 2) ? 0 : src_timing->burst_regs.emc_zcal_interval, 0);
            else if (dram_type != DRAM_TYPE_LPDDR4)
                CcfifoWrite(EMC_ZCAL_INTERVAL,
                            (dst_misc_cfg_2 & 2) ? 0 : dst_timing->burst_regs.emc_zcal_interval, 0);
        }

        if (save_restore_clkstop_pd || (training_enabled && dram_type == DRAM_TYPE_LPDDR4))
            CcfifoWrite(EMC_CFG, dst_timing->burst_regs.emc_cfg & ~(1u << 28), 0);

        if (training_enabled && dram_type == DRAM_TYPE_LPDDR4)
            CcfifoWrite(EMC_SEL_DPD_CTRL, src_timing->emc_sel_dpd_ctrl, 0);

        CcfifoWrite(EMC_DBG, emc_dbg_o, 0);
    }

    /* Step 22 */
    CcfifoWrite(EMC_CFG_PIPE_CLK, emc_cfg_pipe_clk_o, 0);
    CcfifoWrite(EMC_INTSTATUS,    0,                   dst_timing->pipe_clk_delay);

    /* Step 23 */
    if (training_enabled) {
        uint32_t clk_source_emc;
        if (ch0_enable || ch1_enable) {
            clk_source_emc = RegRead(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC);
            RegWrite(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_SAFE, clk_source_emc);
        } else {
            clk_source_emc = 0;
        }
        ChangeDllSrc(src_timing, clk_source_emc);
    }

    {
        uint32_t dig_dll = (RegRead(EMC + EMC_CFG_DIG_DLL) & 0xFFFFFFE4) | 8;
        if ((dst_misc_cfg_2 & 1) == 0)
            dig_dll = (dig_dll & 0xFFFFFF3F) | 0x80;
        RegWrite(EMC + EMC_CFG_DIG_DLL, dig_dll);
        RegRead(EMC + EMC_CFG_DIG_DLL);
    }

    RegRead(MC + MC_EMEM_ADR_CFG);
    RegRead(EMC + EMC_INTSTATUS);

    if (ch0_enable || ch1_enable) {
        RegWrite(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC, dst_clk_src);
        RegRead(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC);
    }

    if (WaitForUpdate(EMC_INTSTATUS, (1u << 4), true, dst_emc_fbio_cfg7))
        return;

    /* Step 24: save training results */
    if (training_enabled) {
        uint32_t tmp_emem_numdev = RegRead(MC + MC_EMEM_ADR_CFG) & 1;
        uint32_t emc_dbg_tmp    = RegRead(EMC + EMC_DBG);
        RegWrite(EMC + EMC_DBG, emc_dbg_tmp | 1);
        uint32_t tmp_dram_dev_num = 1 + tmp_emem_numdev;

        if (train_ca) {
            dst_timing->trim_perch_regs.emc0_cmd_brlshft_0 = RegRead(EMC0 + EMC_CMD_BRLSHFT_0);
            dst_timing->trim_perch_regs.emc1_cmd_brlshft_1 = RegRead(EMC1 + EMC_CMD_BRLSHFT_1);
            dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank0_4 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK0_4);
            dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank0_5 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK0_5);

            if (train_bit_level) {
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd0_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD0_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd0_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD0_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd0_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD0_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd1_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD1_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd1_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD1_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd1_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD1_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd2_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD2_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd2_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD2_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd2_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD2_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd3_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD3_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd3_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD3_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_cmd3_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_CMD3_2);
            }
        }

        if (train_ca_vref) {
            uint32_t emc0_training_opt_ca_vref = RegRead(EMC0 + EMC_TRAINING_OPT_CA_VREF);
            uint32_t emc1_training_opt_ca_vref = RegRead(EMC1 + EMC_TRAINING_OPT_CA_VREF);
            uint32_t rank_mask = (tmp_dram_dev_num == TWO_RANK) ? 0x480C0000 : 0xC80C0000;
            dst_timing->burst_perch_regs.emc0_mrw10 = (emc0_training_opt_ca_vref & 0xFFFF) | 0x880C0000;
            dst_timing->burst_perch_regs.emc1_mrw10 = (emc1_training_opt_ca_vref & 0xFFFF) | 0x880C0000;
            dst_timing->burst_perch_regs.emc0_mrw11 = (rank_mask & 0xFFFFFF00) | ((emc0_training_opt_ca_vref >> 16) & 0xFFFF);
            dst_timing->burst_perch_regs.emc1_mrw11 = (rank_mask & 0xFFFFFF00) | ((emc1_training_opt_ca_vref >> 16) & 0xFFFF);
        }

        if (train_rd) {
            dst_timing->trim_regs.emc_pmacro_ib_ddll_long_dqs_rank0_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_LONG_DQS_RANK0_0);
            dst_timing->trim_regs.emc_pmacro_ib_ddll_long_dqs_rank0_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_LONG_DQS_RANK0_1);
            dst_timing->trim_regs.emc_pmacro_ib_ddll_long_dqs_rank0_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_LONG_DQS_RANK0_2);
            dst_timing->trim_regs.emc_pmacro_ib_ddll_long_dqs_rank0_3 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_LONG_DQS_RANK0_3);

            if (tmp_dram_dev_num == TWO_RANK) {
                dst_timing->trim_regs.emc_pmacro_ib_ddll_long_dqs_rank1_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_LONG_DQS_RANK1_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_long_dqs_rank1_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_LONG_DQS_RANK1_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_long_dqs_rank1_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_LONG_DQS_RANK1_2);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_long_dqs_rank1_3 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_LONG_DQS_RANK1_3);
            }

            if (train_bit_level) {
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte0_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE0_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte0_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE0_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte0_2 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE0_2);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte1_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE1_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte1_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE1_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte1_2 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE1_2);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte2_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE2_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte2_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE2_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte2_2 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE2_2);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte3_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE3_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte3_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE3_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte3_2 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE3_2);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte4_0 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE4_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte4_1 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE4_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte4_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE4_2);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte5_0 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE5_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte5_1 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE5_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte5_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE5_2);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte6_0 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE6_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte6_1 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE6_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte6_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE6_2);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte7_0 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE7_0);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte7_1 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE7_1);
                dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank0_byte7_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK0_BYTE7_2);

                if (tmp_dram_dev_num == TWO_RANK) {
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte0_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE0_0);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte0_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE0_1);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte0_2 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE0_2);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte1_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE1_0);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte1_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE1_1);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte1_2 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE1_2);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte2_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE2_0);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte2_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE2_1);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte2_2 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE2_2);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte3_0 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE3_0);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte3_1 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE3_1);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte3_2 = RegRead(EMC0 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE3_2);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte4_0 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE4_0);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte4_1 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE4_1);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte4_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE4_2);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte5_0 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE5_0);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte5_1 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE5_1);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte5_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE5_2);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte6_0 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE6_0);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte6_1 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE6_1);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte6_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE6_2);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte7_0 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE7_0);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte7_1 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE7_1);
                    dst_timing->trim_regs.emc_pmacro_ib_ddll_short_dq_rank1_byte7_2 = RegRead(EMC1 + EMC_PMACRO_IB_DDLL_SHORT_DQ_RANK1_BYTE7_2);
                }
            }

            if (train_rd_vref) {
                uint32_t emc_pmacro_ib_vref_dq_0 = RegRead(EMC0 + EMC_PMACRO_IB_VREF_DQ_0);
                uint32_t emc_pmacro_ib_vref_dq_1 = RegRead(EMC0 + EMC_PMACRO_IB_VREF_DQ_1);

#define GET_SAVE_RESTORE_MOD_REG(n) \
    ((dst_timing->save_restore_mod_regs[n] & 0x80000000) \
        ? (~dst_timing->save_restore_mod_regs[n]) \
        : (dst_timing->save_restore_mod_regs[n]))

                uint8_t ib_vref_dq_byte0_icr = (uint8_t)(((emc_pmacro_ib_vref_dq_0 >>  0) & 0x7F) + (GET_SAVE_RESTORE_MOD_REG(0) & 0x7F));
                uint8_t ib_vref_dq_byte1_icr = (uint8_t)(((emc_pmacro_ib_vref_dq_0 >>  8) & 0x7F) + (GET_SAVE_RESTORE_MOD_REG(1) & 0x7F));
                uint8_t ib_vref_dq_byte2_icr = (uint8_t)(((emc_pmacro_ib_vref_dq_0 >> 16) & 0x7F) + (GET_SAVE_RESTORE_MOD_REG(2) & 0x7F));
                uint8_t ib_vref_dq_byte3_icr = (uint8_t)(((emc_pmacro_ib_vref_dq_0 >> 24) & 0x7F) + (GET_SAVE_RESTORE_MOD_REG(3) & 0x7F));
                uint8_t ib_vref_dq_byte4_icr = (uint8_t)(((emc_pmacro_ib_vref_dq_1 >>  0) & 0x7F) + (GET_SAVE_RESTORE_MOD_REG(4) & 0x7F));
                uint8_t ib_vref_dq_byte5_icr = (uint8_t)(((emc_pmacro_ib_vref_dq_1 >>  8) & 0x7F) + (GET_SAVE_RESTORE_MOD_REG(5) & 0x7F));
                uint8_t ib_vref_dq_byte6_icr = (uint8_t)(((emc_pmacro_ib_vref_dq_1 >> 16) & 0x7F) + (GET_SAVE_RESTORE_MOD_REG(6) & 0x7F));
                uint8_t ib_vref_dq_byte7_icr = (uint8_t)(((emc_pmacro_ib_vref_dq_1 >> 24) & 0x7F) + (GET_SAVE_RESTORE_MOD_REG(7) & 0x7F));
#undef GET_SAVE_RESTORE_MOD_REG

                dst_timing->trim_regs.emc_pmacro_ib_vref_dq_0 =
                    ((ib_vref_dq_byte0_icr & 0x7F)       ) |
                    ((ib_vref_dq_byte1_icr & 0x7F) <<  8 ) |
                    ((ib_vref_dq_byte2_icr & 0x7F) << 16 ) |
                    ((ib_vref_dq_byte3_icr & 0x7F) << 24 );

                dst_timing->trim_regs.emc_pmacro_ib_vref_dq_1 =
                    ((ib_vref_dq_byte4_icr & 0x7F)       ) |
                    ((ib_vref_dq_byte5_icr & 0x7F) <<  8 ) |
                    ((ib_vref_dq_byte6_icr & 0x7F) << 16 ) |
                    ((ib_vref_dq_byte7_icr & 0x7F) << 24 );
            }
        }

        if (train_wr) {
            dst_timing->trim_perch_regs.emc0_data_brlshft_0 = RegRead(EMC0 + EMC_DATA_BRLSHFT_0);
            dst_timing->trim_perch_regs.emc1_data_brlshft_0 = RegRead(EMC1 + EMC_DATA_BRLSHFT_0);
            if (tmp_dram_dev_num == TWO_RANK) {
                dst_timing->trim_perch_regs.emc0_data_brlshft_1 = RegRead(EMC0 + EMC_DATA_BRLSHFT_1);
                dst_timing->trim_perch_regs.emc1_data_brlshft_1 = RegRead(EMC1 + EMC_DATA_BRLSHFT_1);
            }

            dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank0_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK0_0);
            dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank0_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK0_1);
            dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank0_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK0_2);
            dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank0_3 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK0_3);
            if (tmp_dram_dev_num == TWO_RANK) {
                dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank1_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK1_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank1_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK1_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank1_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK1_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_long_dq_rank1_3 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_LONG_DQ_RANK1_3);
            }

            if (train_bit_level) {
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte0_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte0_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte0_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE0_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte1_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte1_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte1_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE1_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte2_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte2_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte2_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE2_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte3_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte3_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte3_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE3_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte4_0 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte4_1 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte4_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE4_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte5_0 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte5_1 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte5_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE5_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte6_0 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte6_1 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte6_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE6_2);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte7_0 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_0);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte7_1 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_1);
                dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank0_byte7_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK0_BYTE7_2);

                if (tmp_dram_dev_num == TWO_RANK) {
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte0_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_0);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte0_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_1);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte0_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE0_2);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte1_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_0);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte1_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_1);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte1_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE1_2);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte2_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_0);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte2_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_1);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte2_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE2_2);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte3_0 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_0);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte3_1 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_1);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte3_2 = RegRead(EMC0 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE3_2);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte4_0 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_0);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte4_1 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_1);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte4_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE4_2);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte5_0 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_0);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte5_1 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_1);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte5_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE5_2);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte6_0 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_0);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte6_1 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_1);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte6_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE6_2);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte7_0 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_0);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte7_1 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_1);
                    dst_timing->trim_regs.emc_pmacro_ob_ddll_short_dq_rank1_byte7_2 = RegRead(EMC1 + EMC_PMACRO_OB_DDLL_SHORT_DQ_RANK1_BYTE7_2);
                }
            }

            if (train_wr_vref) {
                uint32_t emc0_training_opt_dq_ob_vref = RegRead(EMC0 + EMC_TRAINING_OPT_DQ_OB_VREF);
                uint32_t emc1_training_opt_dq_ob_vref = RegRead(EMC1 + EMC_TRAINING_OPT_DQ_OB_VREF);

#define GET_SAVE_RESTORE_MOD_REG(n) \
    ((dst_timing->save_restore_mod_regs[n] & 0x80000000) \
        ? (~dst_timing->save_restore_mod_regs[n]) \
        : (dst_timing->save_restore_mod_regs[n]))

                uint8_t mod_reg_8  = (uint8_t)GET_SAVE_RESTORE_MOD_REG( 8);
                uint8_t mod_reg_9  = (uint8_t)GET_SAVE_RESTORE_MOD_REG( 9);
                uint8_t mod_reg_10 = (uint8_t)GET_SAVE_RESTORE_MOD_REG(10);
                uint8_t mod_reg_11 = (uint8_t)GET_SAVE_RESTORE_MOD_REG(11);
#undef GET_SAVE_RESTORE_MOD_REG

                uint32_t rank_mask = (tmp_dram_dev_num == TWO_RANK) ? 0x480E0000 : 0xC80E0000;

                uint8_t emc0_mrw12_byte0 = (uint8_t)(mod_reg_8  + ((emc0_training_opt_dq_ob_vref >>  0) & 0xFF));
                uint8_t emc0_mrw12_byte1 = (uint8_t)(mod_reg_9  + ((emc0_training_opt_dq_ob_vref >>  8) & 0xFF));
                uint8_t emc0_mrw13_byte0 = (uint8_t)(mod_reg_8  + ((emc0_training_opt_dq_ob_vref >> 16) & 0xFF));
                uint8_t emc0_mrw13_byte1 = (uint8_t)(mod_reg_9  + ((emc0_training_opt_dq_ob_vref >> 24) & 0xFF));
                uint8_t emc1_mrw12_byte0 = (uint8_t)(mod_reg_10 + ((emc1_training_opt_dq_ob_vref >>  0) & 0xFF));
                uint8_t emc1_mrw12_byte1 = (uint8_t)(mod_reg_11 + ((emc1_training_opt_dq_ob_vref >>  8) & 0xFF));
                uint8_t emc1_mrw13_byte0 = (uint8_t)(mod_reg_10 + ((emc1_training_opt_dq_ob_vref >> 16) & 0xFF));
                uint8_t emc1_mrw13_byte1 = (uint8_t)(mod_reg_11 + ((emc1_training_opt_dq_ob_vref >> 24) & 0xFF));

                dst_timing->burst_perch_regs.emc0_mrw12 = (emc0_mrw12_byte0 << 0) | (emc0_mrw12_byte1 << 8) | 0x880E0000;
                dst_timing->burst_perch_regs.emc1_mrw12 = (emc1_mrw12_byte0 << 0) | (emc1_mrw12_byte1 << 8) | 0x880E0000;
                dst_timing->burst_perch_regs.emc1_mrw13 = (emc1_mrw13_byte0 << 0) | (emc1_mrw13_byte1 << 8) | rank_mask;
                dst_timing->burst_perch_regs.emc0_mrw13 = (emc0_mrw13_byte0 << 0) | (emc0_mrw13_byte1 << 8) | rank_mask;
            }
        }

        RegWrite(EMC + EMC_DBG, emc_dbg_tmp);
    }

    /* Step 25 */
    if (dst_timing->rate_khz > src_timing->rate_khz && !training_enabled) {
        for (uint32_t i = 0; i < dst_timing->num_up_down; ++i)
            RegWrite(LaScaleRegisters[i], dst_timing->la_scale_regs_arr[i]);
        TimingUpdate(dst_emc_fbio_cfg7);
    }

    /* Step 26 */
    if (dram_type == DRAM_TYPE_LPDDR4 && !training_enabled) {
        RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
        RegWrite(EMC + EMC_ZCAL_WAIT_CNT, dst_timing->burst_regs.emc_zcal_wait_cnt);
        RegWrite(EMC + EMC_ZCAL_INTERVAL,
                 (dst_misc_cfg_2 & 2) ? 0 : dst_timing->burst_regs.emc_zcal_interval);
        RegWrite(EMC + EMC_DBG, emc_dbg_o);
    }

    if (dram_type != DRAM_TYPE_LPDDR4 && opt_cc_short_zcal && opt_zcal_en_cc && !opt_short_zcal) {
        usleep(2);
        if (is_lpddr2) {
            RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
            RegWrite(EMC + EMC_MRS_WAIT_CNT, dst_timing->burst_regs.emc_mrs_wait_cnt);
            RegWrite(EMC + EMC_DBG, emc_dbg_o);
        } else if (dram_type == DRAM_TYPE_DDR4) {
            RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
            RegWrite(EMC + EMC_ZCAL_WAIT_CNT, dst_timing->burst_regs.emc_zcal_wait_cnt);
            RegWrite(EMC + EMC_DBG, emc_dbg_o);
        }
    }

    if (training_enabled) {
        if (!src_timing->pllm_misc1_0_pllm_clamp_ph90)
            RegSetBits(CLKRST + CLK_RST_CONTROLLER_PLLM_MISC1, 0x80000000);
    } else {
        if (!dst_timing->pllm_misc1_0_pllm_clamp_ph90)
            RegSetBits(CLKRST + CLK_RST_CONTROLLER_PLLM_MISC1, 0x80000000);
    }

    /* Step 27 */
    RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
    RegWrite(EMC + EMC_CFG, dst_timing->burst_regs.emc_cfg);
    RegWrite(EMC + EMC_DBG, emc_dbg_o);
    RegWrite(EMC + EMC_FDPD_CTRL_CMD_NO_RAMP, dst_timing->emc_fdpd_ctrl_cmd_no_ramp);
    RegWrite(EMC + EMC_SEL_DPD_CTRL,          dst_timing->emc_sel_dpd_ctrl);

    /* Step 28 */
    if (training_enabled && dram_type == DRAM_TYPE_LPDDR4) {
        RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
        RegWrite(EMC + EMC_CFG,          dst_timing->burst_regs.emc_cfg);
        RegWrite(EMC + EMC_SEL_DPD_CTRL, dst_timing->emc_sel_dpd_ctrl);
        RegWrite(EMC + EMC_ZCAL_WAIT_CNT, src_timing->burst_regs.emc_zcal_wait_cnt);
        RegWrite(EMC + EMC_ZCAL_INTERVAL,
                 (dst_misc_cfg_2 & 2) ? 0 : dst_timing->burst_regs.emc_zcal_interval);
        RegWrite(EMC + EMC_AUTO_CAL_CONFIG2, dst_timing->emc_auto_cal_config2);
        if (ch0_enable || ch1_enable) {
            RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG3, dst_timing->emc_auto_cal_config3);
            RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG4, dst_timing->emc_auto_cal_config4);
            RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG5, dst_timing->emc_auto_cal_config5);
            RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG6, dst_timing->emc_auto_cal_config6);
            RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG7, dst_timing->emc_auto_cal_config7);
            RegWrite(EMC0 + EMC_AUTO_CAL_CONFIG8, dst_timing->emc_auto_cal_config8);
        }
        RegWrite(EMC + EMC_DBG, emc_dbg_o);
        RegWrite(EMC + EMC_TR_DVFS, dst_timing->burst_regs.emc_tr_dvfs & ~(1u << 0));
    }

    /* Step 29 */
    RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));
    RegWrite(EMC + EMC_PMACRO_AUTOCAL_CFG_COMMON, dst_timing->burst_regs.emc_pmacro_autocal_cfg_common);
    RegWrite(EMC + EMC_DBG, emc_dbg_o);

    RegWrite(EMC + EMC_PMACRO_CFG_PM_GLOBAL_0, 0xFF0000);
    RegWrite(EMC + EMC_PMACRO_TRAINING_CTRL_0, 0x8);
    RegWrite(EMC + EMC_PMACRO_TRAINING_CTRL_1, 0x8);
    RegWrite(EMC + EMC_PMACRO_CFG_PM_GLOBAL_0, 0);

    RegWrite(EMC + EMC_DBG, SetShadowBypass(ACTIVE, emc_dbg_o));

    if ((dst_misc_cfg_1 & 0x20) == 0) {
        uint32_t xm2comppadctrl = RegRead(EMC + EMC_XM2COMPPADCTRL);
        RegWrite(EMC + EMC_XM2COMPPADCTRL, xm2comppadctrl & 0x1CFFFFFF); usleep(1);
        RegWrite(EMC + EMC_XM2COMPPADCTRL, xm2comppadctrl & 0x0CFFFFFF); usleep(1);
        RegWrite(EMC + EMC_XM2COMPPADCTRL, xm2comppadctrl & 0x04FFFFFF); usleep(1);
    }

    RegSetBits(EMC + EMC_PMACRO_DLL_CFG_1, 0x2000);
    RegRead(EMC + EMC_PMACRO_DLL_CFG_1);
    usleep(2);

    RegSetBits(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL, 0xC00);
    RegRead(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC_DLL);

    RegWrite(EMC + EMC_DBG, SetShadowBypass(ASSEMBLY, emc_dbg_o));
    RegRead(EMC + EMC_DBG);

    /* Step 30 */
    if (!training_enabled) {
        if (dst_timing->burst_regs.emc_cfg_dig_dll & 1) {
            uint32_t dig_dll = (RegRead(EMC + EMC_CFG_DIG_DLL) & 0xFFFFFFE4) | 9;
            if ((dst_misc_cfg_2 & 1) == 0)
                dig_dll = (dig_dll & 0xFFFFFF3F) | 0x80;
            RegWrite(EMC + EMC_CFG_DIG_DLL, dig_dll);
            TimingUpdate(dst_emc_fbio_cfg7);
        }
    }

    if (!(training_enabled && dram_type == DRAM_TYPE_LPDDR4))
        RegWrite(EMC + EMC_AUTO_CAL_CONFIG,
                 training_enabled ? src_timing->emc_auto_cal_config : dst_timing->emc_auto_cal_config);

    if (training_enabled)
        g_fsp_for_next_freq = !g_fsp_for_next_freq;

    if (src_timing->periodic_training) {
        src_timing->current_dram_clktree_c0d0u0 = src_timing->trained_dram_clktree_c0d0u0;
        src_timing->current_dram_clktree_c0d0u1 = src_timing->trained_dram_clktree_c0d0u1;
        src_timing->current_dram_clktree_c0d1u0 = src_timing->trained_dram_clktree_c0d1u0;
        src_timing->current_dram_clktree_c0d1u1 = src_timing->trained_dram_clktree_c0d1u1;
        src_timing->current_dram_clktree_c1d0u0 = src_timing->trained_dram_clktree_c1d0u0;
        src_timing->current_dram_clktree_c1d0u1 = src_timing->trained_dram_clktree_c1d0u1;
        src_timing->current_dram_clktree_c1d1u0 = src_timing->trained_dram_clktree_c1d1u0;
        src_timing->current_dram_clktree_c1d1u1 = src_timing->trained_dram_clktree_c1d1u1;
    }

    PllDisable(dst_clk_src);
}

static void CleanupActiveShadowCopy(EmcDvfsTimingTable *src_timing, EmcDvfsTimingTable *dst_timing) {
    uint32_t emc_dbg = RegRead(EMC + EMC_DBG);
    emc_dbg = (emc_dbg & 0xF3FFFFFF) | 0x8000000;
    RegWrite(EMC + EMC_DBG, emc_dbg);

    TimingUpdate(src_timing->emc_fbio_cfg7);

    emc_dbg = RegRead(EMC + EMC_DBG);
    emc_dbg &= 0xF3FFFFFF;
    RegWrite(EMC + EMC_DBG, emc_dbg);

    RegWrite(EMC + EMC_PMACRO_DLL_CFG_1,
             (RegRead(EMC + EMC_PMACRO_DLL_CFG_1) & 0x00002000) |
             (src_timing->burst_regs.emc_pmacro_dll_cfg_1 & 0xFFFFDFFF));

    uint32_t emc_cfg_dig_dll = RegRead(EMC + EMC_CFG_DIG_DLL);
    emc_cfg_dig_dll = (dst_timing->misc_cfg_2 & 1)
        ? (emc_cfg_dig_dll & 0xFFFFFFFE)
        : ((emc_cfg_dig_dll & 0xFFFFFF3E) | 0x80);
    RegWrite(EMC + EMC_CFG_DIG_DLL, emc_cfg_dig_dll);

    TimingUpdate(src_timing->emc_fbio_cfg7);

    emc_cfg_dig_dll = RegRead(EMC + EMC_CFG_DIG_DLL);
    if (src_timing->burst_regs.emc_cfg_dig_dll & 1)
        emc_cfg_dig_dll |= 0x01;
    else
        emc_cfg_dig_dll &= 0xFFFFFFFE;
    if ((dst_timing->misc_cfg_2 & 1) == 0)
        emc_cfg_dig_dll = (emc_cfg_dig_dll & 0xFFFFFF3F) | 0x80;
    RegWrite(EMC + EMC_CFG_DIG_DLL, emc_cfg_dig_dll);

    TimingUpdate(src_timing->emc_fbio_cfg7);

    WaitForUpdate(EMC_DIG_DLL_STATUS, (1 << 2), true, src_timing->emc_fbio_cfg7);

    TimingUpdate(src_timing->emc_fbio_cfg7);

    uint32_t emc_auto_cal_config = RegRead(EMC + EMC_AUTO_CAL_CONFIG);
    emc_auto_cal_config = (dst_timing->misc_cfg_2 & 4)
        ? (emc_auto_cal_config & 0xDFFFF9FF)
        : ((emc_auto_cal_config & 0x5FFFF9FF) | 0x80000000);
    RegWrite(EMC + EMC_AUTO_CAL_CONFIG, emc_auto_cal_config | 0x20000000);
}

static void TrainFreq(EmcDvfsTimingTable *src_timing, EmcDvfsTimingTable *dst_timing,
                       uint32_t next_clk_src) {
    const uint32_t dram_dev_num = (RegRead(MC + MC_EMEM_ADR_CFG) & 1) + 1;

    if (!g_did_first_training) {
        const EmcRamTrainingPattern *pattern = GetEmcRamTrainingPattern();
        for (uint32_t i = 0; i < 0x100; ++i) {
            RegWrite(EMC + EMC_TRAINING_PATRAM_DQ,   pattern[dst_timing->training_pattern].dq[i]);
            RegWrite(EMC + EMC_TRAINING_PATRAM_DMI,  pattern[dst_timing->training_pattern].dmi[i]);
            RegWrite(EMC + EMC_TRAINING_PATRAM_CTRL, 0x80000000 | i);
        }
        g_did_first_training = true;
    }

    RegWrite(EMC + EMC_TRAINING_QUSE_CTRL_MISC,
             (dst_timing->burst_regs.emc_training_read_ctrl_misc & 0xFFFF0000) | 0x00001000);

    const uint32_t needed_training = dst_timing->needs_training;
    if (needed_training && !dst_timing->trained) {
        uint32_t training_params[4];
        uint32_t num_params = 0;

        if (needed_training & (CA_TRAINING | CA_VREF_TRAINING)) {
            training_params[num_params++] = (needed_training & (CA_TRAINING | CA_VREF_TRAINING | BIT_LEVEL_TRAINING));
            if (dram_dev_num == TWO_RANK)
                training_params[num_params++] = (needed_training & (CA_TRAINING | CA_VREF_TRAINING | TRAIN_SECOND_RANK | BIT_LEVEL_TRAINING));
        }

        if (needed_training & (WRITE_TRAINING | WRITE_VREF_TRAINING | READ_TRAINING | READ_VREF_TRAINING))
            training_params[num_params++] = (needed_training & (WRITE_TRAINING | WRITE_VREF_TRAINING | READ_TRAINING | READ_VREF_TRAINING | BIT_LEVEL_TRAINING));

        for (uint32_t i = 0; i < num_params; ++i) {
            FreqChange(src_timing, dst_timing, training_params[i], next_clk_src);
            CleanupActiveShadowCopy(src_timing, dst_timing);
        }

        dst_timing->trained = 1;
    }
}

static void Dvfs(EmcDvfsTimingTable *dst_timing, EmcDvfsTimingTable *src_timing, bool train) {
    uint32_t clk_src_emc_from = src_timing->clk_src_emc;
    uint32_t clk_src_emc_to   = dst_timing->clk_src_emc;
    uint32_t rate_from        = src_timing->rate_khz;
    uint32_t rate_to          = dst_timing->rate_khz;

    const bool ch0_enable = RegGetField(dst_timing->emc_fbio_cfg7, EMC_REG_BITS_MASK(FBIO_CFG7_CH0_ENABLE)) == EMC_FBIO_CFG7_CH0_ENABLE_ENABLE;
    const bool ch1_enable = RegGetField(dst_timing->emc_fbio_cfg7, EMC_REG_BITS_MASK(FBIO_CFG7_CH1_ENABLE)) == EMC_FBIO_CFG7_CH1_ENABLE_ENABLE;

    const uint32_t prev_2x_clk_src = RegGetField(clk_src_emc_from, CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC));
    const uint32_t next_2x_clk_src = RegGetField(clk_src_emc_to,   CLK_RST_REG_BITS_MASK(CLK_SOURCE_EMC_EMC_2X_CLK_SRC));

    if (next_2x_clk_src != PLLP_OUT0 && next_2x_clk_src != PLLP_UD) {
        if (ch0_enable || ch1_enable) {
            if (PllReprogram(rate_to, clk_src_emc_to, rate_from, clk_src_emc_from)) {
                if (prev_2x_clk_src == PLLMB_UD || prev_2x_clk_src == PLLMB_OUT0) {
                    g_next_pll = 0;
                } else if (prev_2x_clk_src == PLLM_UD || prev_2x_clk_src == PLLM_OUT0) {
                    g_next_pll = !g_next_pll;
                }
                clk_src_emc_to = ProgramPllm(rate_to, clk_src_emc_to, clk_src_emc_to, g_next_pll, dst_timing);
            } else {
                if (next_2x_clk_src == PLLM_UD || next_2x_clk_src == PLLMB_UD) {
                    if (g_next_pll)
                        clk_src_emc_to = RegSetField(clk_src_emc_to, CLK_RST_REG_BITS_VALUE(CLK_SOURCE_EMC_EMC_2X_CLK_SRC, PLLMB_UD));
                } else if (next_2x_clk_src == PLLM_OUT0 || next_2x_clk_src == PLLMB_OUT0) {
                    if (g_next_pll)
                        clk_src_emc_to = RegSetField(clk_src_emc_to, CLK_RST_REG_BITS_VALUE(CLK_SOURCE_EMC_EMC_2X_CLK_SRC, PLLMB_OUT0));
                }
            }
        }
    }

    if (train) {
        TrainFreq(src_timing, dst_timing, clk_src_emc_to);
        if (ch0_enable || ch1_enable) {
            if (PllReprogram(dst_timing->rate_khz, dst_timing->clk_src_emc,
                             src_timing->rate_khz, src_timing->clk_src_emc))
                g_next_pll = !g_next_pll;
        }
    } else {
        FreqChange(src_timing, dst_timing, 0, clk_src_emc_to);
    }
}

static const s8 MemoryTrainingTableIndex_Invalid = ~0;
static const s8 MemoryTrainingTableIndices[] = {
    /* DramId_EristaIcosaSamsung4gb    */ 0x00,
    /* DramId_EristaIcosaHynix4gb      */ 0x02,
    /* DramId_EristaIcosaMicron4gb     */ 0x03,
    /* DramId_MarikoIowaHynix1y4gb     */ 0x10,
    /* DramId_EristaIcosaSamsung6gb    */ 0x01,
    /* DramId_MarikoHoagHynix1y4gb     */ 0x10,
    /* DramId_MarikoAulaHynix1y4gb     */ 0x10,
    /* DramId_MarikoIowax1x2Samsung4gb */ 0x00,
    /* DramId_MarikoIowaSamsung4gb     */ 0x05,
    /* DramId_MarikoIowaSamsung8gb     */ 0x06,
    /* DramId_MarikoIowaHynix4gb       */ 0x07,
    /* DramId_MarikoIowaMicron4gb      */ 0x08,
    /* DramId_MarikoHoagSamsung4gb     */ 0x05,
    /* DramId_MarikoHoagSamsung8gb     */ 0x06,
    /* DramId_MarikoHoagHynix4gb       */ 0x07,
    /* DramId_MarikoHoagMicron4gb      */ 0x08,
    /* DramId_MarikoIowaSamsung4gbY    */ 0x09,
    /* DramId_MarikoIowaSamsung1y4gbX  */ 0x0C,
    /* DramId_MarikoIowaSamsung1y8gbX  */ 0x0D,
    /* DramId_MarikoHoagSamsung1y4gbX  */ 0x0C,
    /* DramId_MarikoIowaSamsung1z4gb   */ 0x12,
    /* DramId_MarikoHoagSamsung1z4gb   */ 0x12,
    /* DramId_MarikoAulaSamsung1z4gb   */ 0x12,
    /* DramId_MarikoHoagSamsung1y8gbX  */ 0x0D,
    /* DramId_MarikoAulaSamsung1y4gbX  */ 0x0C,
    /* DramId_MarikoIowaMicron1y4gb    */ 0x0F,
    /* DramId_MarikoHoagMicron1y4gb    */ 0x0F,
    /* DramId_MarikoAulaMicron1y4gb    */ 0x0F,
    /* DramId_MarikoAulaSamsung1y8gbX  */ 0x0D,
    /* DramId_MarikoIowaHynix1a4gb     */ 0x13,
    /* DramId_MarikoHoagHynix1a4gb     */ 0x13,
    /* DramId_MarikoAulaHynix1a4gb     */ 0x13,
    /* DramId_MarikoIowaMicron1a4gb    */ 0x14,
    /* DramId_MarikoHoagMicron1a4gb    */ 0x14,
    /* DramId_MarikoAulaMicron1a4gb    */ 0x14,
};

int GetMemoryTrainingTableIndex() {
    if (const int dram_id = fuse_read_dramid(true); dram_id < (int) sizeof(MemoryTrainingTableIndices) && MemoryTrainingTableIndices[dram_id] != MemoryTrainingTableIndex_Invalid) {
        return (int) MemoryTrainingTableIndices[dram_id];
    } else {
        return -1;
    }
}

void MarikoTrainMemory(bool *out_did_training) {
    int index = GetMemoryTrainingTableIndex();

    if (index == MemoryTrainingTableIndex_Invalid) {
        EPRINTF("Invalid mtc index");
        *out_did_training = false;
        return;
    }

    EmcDvfsTimingTable *timing     = GetEmcDvfsTimingTables(index, mtc_tables_buffer);
    EmcDvfsTimingTable *src_timing = timing + 0;
    EmcDvfsTimingTable *dst_timing = timing + 1;

    if (src_timing->rate_khz != 204000 || dst_timing->rate_khz != 1600000) {
        EPRINTF("Emc table seems corrupt");
        *out_did_training = false;
        return;
    }
    // TODO: figure out what causes this to fail
    // if (src_timing->clk_src_emc != RegRead(CLKRST + CLK_RST_CONTROLLER_CLK_SOURCE_EMC)) {
    //     EPRINTF("CLK Source Mismatch");
    //     *out_did_training = false;
    //     return;
    // }

    Dvfs(dst_timing, src_timing, true);
    msleep(100);
    Dvfs(dst_timing, src_timing, false);

    g_periodic_timing = dst_timing;

    *out_did_training = true;
}

void MarikoPeriodicTraining(void) {
    if (g_periodic_timing == NULL) {
        return;
    }

    int dram_dev_num = (int)((RegRead(MC + MC_EMEM_ADR_CFG) & 1) + 1);

    PeriodicCompensationHandler(PERIODIC_TRAINING_SEQUENCE, (uint32_t)dram_dev_num,
                                g_periodic_timing, g_periodic_timing);
}

void RestoreMemoryClockRateMariko() {

    /* No longer at the trained rate, so periodic training must stop. */
    g_periodic_timing = NULL;


    int index = GetMemoryTrainingTableIndex();

    if (index == MemoryTrainingTableIndex_Invalid) {
        EPRINTF("Invalid mtc index");
        return;
    }

    EmcDvfsTimingTable *timing_tables = GetEmcDvfsTimingTables(index, mtc_tables_buffer);
    EmcDvfsTimingTable *src_timing    = timing_tables + 0;
    EmcDvfsTimingTable *dst_timing    = timing_tables + 1;

    if (src_timing->rate_khz != 204000 || dst_timing->rate_khz != 1600000) {
        EPRINTF("Emc table seems corrupt");
        return;
    }

    Dvfs(src_timing, dst_timing, false);
}