#pragma once

bool checkfile(char* path);
void viewbytes(char *path);
int copy(const char *locin, const char *locout, bool print, bool canCancel);
u64 getfilesize(char *path);
int getfolderentryamount(const char *path);
void makestring(char *in, char **out);
int del_recursive(char *path);
int copy_recursive(char *path, char *dstpath);