#pragma once

#define OPTION1 (1 << 0)
#define OPTION2 (1 << 1)
#define OPTION3 (1 << 2)
#define OPTION4 (1 << 3)

#define PATHSIZE 512

void utils_gfx_init();
void utils_waitforpower();
void removepartpath(char *path);
void addpartpath(char *path, char *add);
int readfolder(char *items[], unsigned int *muhbits, const char *path);
int copy(const char *src, const char *dst);
void addchartoarray(char *add, char *items[], int spot);
int copywithpath(const char *src, const char *dstpath, int mode);
void return_readable_byte_amounts(unsigned long int size, char *in);
int getfilesize(const char *path);