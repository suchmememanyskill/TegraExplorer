For future reference if updating the BDK

// TODO: fatfs, ini parser, memory map

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