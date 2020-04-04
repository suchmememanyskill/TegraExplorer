#pragma once
#include "../../utils/types.h"

#define ISDIR (1 << 0)
#define ISARC (1 << 1)

#define ISHIDE (1 << 8)
#define ISMENU (1 << 9)
#define ISSKIP (1 << 10)

#define ISGB (1 << 7)
#define ISMB (1 << 6)
#define ISKB (1 << 5)
#define ISB  (1 << 4)

#define SETBIT(object, shift, value) ((value) ? (object |= shift) : (object &= ~shift))

/* Bit table for property as a file:
0000 0001: Directory bit
0000 0010: Archive bit (or hideme)
0001 0000: Size component is a Byte
0010 0000: Size component is a KiloByte
0100 0000: Size component is a MegaByte
1000 0000: Size component is a GigaByte : note that this won't surpass gigabytes, but i don't expect people to have a single file that's a terrabyte big
*/

#define COPY_MODE_PRINT 0x1
#define COPY_MODE_CANCEL 0x2
#define BUFSIZE 32768

#define OPERATIONCOPY 0x2
#define OPERATIONMOVE 0x4

#define PART_BOOT 0x1
#define PART_PKG2 0x2

#define BOOT0_ARG 0x80
#define BOOT1_ARG 0x40
#define BCPKG2_1_ARG 0x20
#define BCPKG2_3_ARG 0x10

typedef struct {
    char *name;
    u32 storage;
    u16 property;
} menu_entry; 

typedef struct {
    const char *name;
    u8 property;
} gpt_entry_rule;

#define isFS 0x80
#define isBOOT 0x2