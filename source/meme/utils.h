#pragma once

#define OPTION1 (1 << 0)
#define OPTION2 (1 << 1)
#define OPTION3 (1 << 2)
#define OPTION4 (1 << 3)

void utils_gfx_init();
void utils_waitforpower();
void removepartpath(char *path);
void addpartpath(char *path, char *add);
int readfolder(char *items[], unsigned int *muhbits, const char *path);