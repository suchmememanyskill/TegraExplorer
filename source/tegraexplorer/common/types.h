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

typedef struct {
    char *name;
    u32 storage;
    u16 property;
} menu_entry; 