#pragma once
#include "../utils/types.h"

#define ISDIR (1 << 0)
#define ISARC (1 << 1)

#define ISGB (1 << 7)
#define ISMB (1 << 6)
#define ISKB (1 << 5)
#define ISB  (1 << 4)

#define OPERATIONCOPY (1 << 1)
#define OPERATIONMOVE (1 << 2)

#define BUFSIZE 32768

/* Bit table for property:
0000 0001: Directory bit
0000 0010: Archive bit
0001 0000: Size component is a Byte
0010 0000: Size component is a KiloByte
0100 0000: Size component is a MegaByte
1000 0000: Size component is a GigaByte : note that this won't surpass gigabytes, but i don't expect people to have a single file that's a terrabyte big
*/

typedef struct _fs_entry {
    char* name;
    u16 size;
    u8 property;
} fs_entry;

enum filemenuoptions {
    COPY = 1,
    MOVE,
    DELETE,
    PAYLOAD,
    HEXVIEW
};

int readfolder(const char *path);
void filemenu();
bool checkfile(char* path);
u64 getfilesize(char *path);