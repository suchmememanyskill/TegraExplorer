#pragma once
#include "../../utils/types.h"

#define BIT(n) (1U << n)

#define ISDIR BIT(0)
#define ISMENU BIT(1)
#define SETSIZE(x) (x << 2)
#define ISNULL BIT(4)
#define ISHIDE BIT(5)
#define ISSKIP BIT(6)

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
    union {
        u8 property;
        struct {
            u8 isDir:1;
            u8 isMenu:1;
            u8 size:2;
            u8 isNull:1;
            u8 isHide:1;
            u8 isSkip:1;
        };
    };
} menu_entry; 

typedef struct {
    const char *name;
    u8 property;
} gpt_entry_rule;

#define isFS 0x80
#define isBOOT 0x2