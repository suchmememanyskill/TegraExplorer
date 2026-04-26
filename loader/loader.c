/*
* Copyright (c) 2019-2024 CTCaer
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

#include <string.h>
#include <stdlib.h>

#include "../loader/payload.h"

#include <memory_map.h>
#include <../bdk/libs/compr/nrv/nrv2e.h>
#include <soc/bpmp.h>
#include <soc/clock.h>
#include <soc/t210.h>

// 0x4003D000: Safe for panic preserving, 0x40038000: Safe for debugging needs.
#define IPL_RELOC_TOP        0x40038000
#define IPL_PATCHED_RELOC_SZ 0x94
#define IPL_VERSION_RCFG_OFF 0x120

boot_cfg_t __attribute__((section ("._boot_cfg"))) b_cfg;

void loader_main()
{
	// Preliminary BPMP clocks init.
	CLOCK(CLK_RST_CONTROLLER_CLK_SYSTEM_RATE) = 0x10;          // Set HCLK div to 2 and PCLK div to 1.
	CLOCK(CLK_RST_CONTROLLER_CLK_SOURCE_SYS) = 0;              // Set SCLK div to 1.
	CLOCK(CLK_RST_CONTROLLER_SCLK_BURST_POLICY) = 0x20004444;  // Set clk source to Run and PLLP_OUT2 (204MHz).
	CLOCK(CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER) = 0x80000000; // Enable SUPER_SDIV to 1.
	CLOCK(CLK_RST_CONTROLLER_CLK_SYSTEM_RATE) = 2;             // Set HCLK div to 1 and PCLK div to 3.
	CLOCK(CLK_RST_CONTROLLER_SCLK_BURST_POLICY) = 0x20003333;  // Set SCLK to PLLP_OUT (408MHz).


	// Get Payload size.
	u32 payload_size  = sizeof(payload);               // Actual payload size.
	payload_size      = ALIGN(payload_size, 4);        // Align size to 4 bytes.
	u32 *payload_addr = (u32 *)payload;

	// Relocate payload to a safer place.
	u32 words = payload_size >> 2;
	u32 *src  = payload_addr + words - 1;
	u32 *dst  = (u32 *)(IPL_RELOC_TOP - 4);
	while (words)
	{
		*dst = *src;
		src--;
		dst--;
		words--;
	}

	// Set source address of the first part.
	u8 *src_addr = (void *)(IPL_RELOC_TOP - payload_size);

	// Uncompress.
	u32 out_len;
	nrv2e_decompress_8(src_addr, sizeof(payload), (u8 *)IPL_LOAD_ADDR, &out_len);

	// Copy new reserved configuration.
	memcpy((u8 *)(IPL_LOAD_ADDR + IPL_PATCHED_RELOC_SZ), &b_cfg, sizeof(boot_cfg_t));

	// Chainload into uncompressed payload.
	void (*ipl_ptr)() = (void *)IPL_LOAD_ADDR;
	(*ipl_ptr)();

	// Halt if we managed to get out of execution.
	while (true)
		;
}
