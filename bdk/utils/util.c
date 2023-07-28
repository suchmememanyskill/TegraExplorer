/*
* Copyright (c) 2018 naehrwert
* Copyright (c) 2018-2020 CTCaer
# Copyright (c) 2022 shchmue
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

#include <utils/util.h>
#include <mem/heap.h>
#include <power/max77620.h>
#include <rtc/max77620-rtc.h>
#include <soc/bpmp.h>
#include <soc/hw_init.h>
#include <soc/i2c.h>
#include <soc/pmc.h>
#include <soc/t210.h>
#include <storage/nx_sd.h>

#define USE_RTC_TIMER

extern volatile nyx_storage_t *nyx_str;

u8 bit_count(u32 val)
{
	u8 cnt = 0;
	for (u32 i = 0; i < 32; i++)
	{
		if ((val >> i) & 1)
			cnt++;
	}

	return cnt;
}

u32 bit_count_mask(u8 bits)
{
	u32 val = 0;
	for (u32 i = 0; i < bits; i++)
		val |= 1 << i;

	return val;
}

u32 get_tmr_s()
{
	return RTC(APBDEV_RTC_SECONDS);
}

u32 get_tmr_ms()
{
	// The registers must be read with the following order:
	// RTC_MILLI_SECONDS (0x10) -> RTC_SHADOW_SECONDS (0xC)
	return (RTC(APBDEV_RTC_MILLI_SECONDS) + (RTC(APBDEV_RTC_SHADOW_SECONDS) * 1000));
}

u32 get_tmr_us()
{
	return TMR(TIMERUS_CNTR_1US);
}

void msleep(u32 ms)
{
#ifdef USE_RTC_TIMER
	u32 start = RTC(APBDEV_RTC_MILLI_SECONDS) + (RTC(APBDEV_RTC_SHADOW_SECONDS) * 1000);
	// Casting to u32 is important!
	while (((u32)(RTC(APBDEV_RTC_MILLI_SECONDS) + (RTC(APBDEV_RTC_SHADOW_SECONDS) * 1000)) - start) <= ms)
		;
#else
	bpmp_msleep(ms);
#endif
}

void usleep(u32 us)
{
#ifdef USE_RTC_TIMER
	u32 start = TMR(TIMERUS_CNTR_1US);

	// Check if timer is at upper limits and use BPMP sleep so it doesn't wake up immediately.
	if ((start + us) < start)
		bpmp_usleep(us);
	else
		while ((u32)(TMR(TIMERUS_CNTR_1US) - start) <= us) // Casting to u32 is important!
			;
#else
	bpmp_usleep(us);
#endif
}

void exec_cfg(u32 *base, const cfg_op_t *ops, u32 num_ops)
{
	for(u32 i = 0; i < num_ops; i++)
		base[ops[i].off] = ops[i].val;
}

u16 crc16_calc(const u8 *buf, u32 len)
{
	const u8 *p, *q;
	u16 crc = 0x55aa;

	static u16 table[16] = {
		0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
		0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400
	};

	q = buf + len;
	for (p = buf; p < q; p++)
	{
		u8 oct = *p;
		crc = (crc >> 4) ^ table[crc & 0xf] ^ table[(oct >> 0) & 0xf];
		crc = (crc >> 4) ^ table[crc & 0xf] ^ table[(oct >> 4) & 0xf];
	}

	return crc;
}

u32 crc32_calc(u32 crc, const u8 *buf, u32 len)
{
	const u8 *p, *q;
	static u32 *table = NULL;

	// Calculate CRC table.
	if (!table)
	{
		table = calloc(256, sizeof(u32));
		for (u32 i = 0; i < 256; i++)
		{
			u32 rem = i;
			for (u32 j = 0; j < 8; j++)
			{
				if (rem & 1)
				{
					rem >>= 1;
					rem ^= 0xedb88320;
				}
				else
					rem >>= 1;
			}
			table[i] = rem;
		}
	}

	crc = ~crc;
	q = buf + len;
	for (p = buf; p < q; p++)
	{
		u8 oct = *p;
		crc = (crc >> 8) ^ table[(crc & 0xff) ^ oct];
	}

	return ~crc;
}

void panic(u32 val)
{
	// Set panic code.
	PMC(APBDEV_PMC_SCRATCH200) = val;
	//PMC(APBDEV_PMC_CRYPTO_OP) = PMC_CRYPTO_OP_SE_DISABLE;
	TMR(TIMER_WDT4_UNLOCK_PATTERN) = TIMER_MAGIC_PTRN;
	TMR(TIMER_TMR9_TMR_PTV) = TIMER_EN | TIMER_PER_EN;
	TMR(TIMER_WDT4_CONFIG)  = TIMER_SRC(9) | TIMER_PER(1) | TIMER_PMCRESET_EN;
	TMR(TIMER_WDT4_COMMAND) = TIMER_START_CNT;

	while (true)
		usleep(1);
}

void power_set_state(power_state_t state)
{
	u8 reg;

	// Unmount and power down sd card.
	sd_end();

	// De-initialize and power down various hardware.
	hw_reinit_workaround(false, 0);

	// Stop the alarm, in case we injected and powered off too fast.
	max77620_rtc_stop_alarm();

	// Set power state.
	switch (state)
	{
	case REBOOT_RCM:
		PMC(APBDEV_PMC_SCRATCH0) = PMC_SCRATCH0_MODE_RCM; // Enable RCM path.
		PMC(APBDEV_PMC_CNTRL)   |= PMC_CNTRL_MAIN_RST;    // PMC reset.
		break;

	case REBOOT_BYPASS_FUSES:
		panic(0x21); // Bypass fuse programming in package1.
		break;

	case POWER_OFF:
		// Initiate power down sequence and do not generate a reset (regulators retain state).
		i2c_send_byte(I2C_5, MAX77620_I2C_ADDR, MAX77620_REG_ONOFFCNFG1, MAX77620_ONOFFCNFG1_PWR_OFF);
		break;

	case POWER_OFF_RESET:
	case POWER_OFF_REBOOT:
	default:
		// Enable/Disable soft reset wake event.
		reg = i2c_recv_byte(I2C_5, MAX77620_I2C_ADDR, MAX77620_REG_ONOFFCNFG2);
		if (state == POWER_OFF_RESET) // Do not wake up after power off.
			reg &= ~(MAX77620_ONOFFCNFG2_SFT_RST_WK | MAX77620_ONOFFCNFG2_WK_ALARM1 | MAX77620_ONOFFCNFG2_WK_ALARM2);
		else // POWER_OFF_REBOOT. Wake up after power off.
			reg |= MAX77620_ONOFFCNFG2_SFT_RST_WK;
		i2c_send_byte(I2C_5, MAX77620_I2C_ADDR, MAX77620_REG_ONOFFCNFG2, reg);

		// Initiate power down sequence and generate a reset (regulators' state resets).
		i2c_send_byte(I2C_5, MAX77620_I2C_ADDR, MAX77620_REG_ONOFFCNFG1, MAX77620_ONOFFCNFG1_SFT_RST);
		break;
	}

	while (true)
		bpmp_halt();
}

void power_set_state_ex(void *param)
{
	power_state_t *state = (power_state_t *)param;
	power_set_state(*state);
}
