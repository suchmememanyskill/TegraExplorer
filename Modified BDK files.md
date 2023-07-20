For future reference if updating the BDK

// TODO: memory map(?)

## /bdk/sec/SE.c&h
- Added se_aes_cmac
- Added se_calc_hmac_sha256

## /bdk/usb
- Removed entirely

## /bdk/storage/sd.c
- in sd_file_read(), extend read buffer by 1 and place a NULL byte at the end

## /bdk/libs/fatfs/ff.c&h
- Added f_fdisk_mod
- Stubbed exfat partition creation

## /bdk/utils/ini.c
- Added initial 'unknown' section to parse prod.keys

## /bdk/utils/types.h
- Added colors from lockpick-rcm
- Add ALWAYS_INLINE, LOG2, DIV_ROUND_UP defs
- Add 'open_mode_t' and 'validity_t' enums

## /bdk/libs
- Added nx-savedata from [Lockpick_RCM](https://github.com/shchmue/Lockpick_RCM)