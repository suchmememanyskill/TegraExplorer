#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nrv.h"



int main(int argc, char *argv[])
{
    char filename[1024];
    int filename_len;

    struct stat statbuf;
    FILE *in_file, *out_file;

    int level = 10;                  /* compression level (1-10) */

    int r;
    byte *in = NULL;
    byte *out = NULL;
    u32 in_len;
    u32 out_len;

    u32 new_len;


    if(stat(argv[1], &statbuf))
        goto error;

    if((in_file=fopen(argv[1], "rb")) == NULL)
        goto error;

    strcpy(filename, argv[1]);
    filename_len = strlen(filename);

    in_len = statbuf.st_size;

    in = (byte *) malloc(in_len);
    out = (byte *) malloc(in_len + in_len / 8 + 256);

    if (in == NULL || out == NULL)
    {
        printf("out of memory\n");
        return 3;
    }

    if (fread(in, 1, in_len, in_file) != in_len)
        goto error;

    fclose(in_file);

    strcpy(filename + filename_len, ".nrv");

    r = nrv2e_99_compress(in, in_len, out, &out_len, level, NULL,NULL);
    if (r != 0) {
        printf("nrv2e_99_compress failed, result code: %d\n", r);
        goto error;
    }

    if((out_file = fopen(filename,"wb")) == NULL)
        goto error;

    if (fwrite(out, 1, out_len, out_file) != out_len)
        goto error;

    fclose(out_file);
    if (out_len >= in_len)
        goto error;


    free(out);
    free(in);

    return 0;

error:
    if (out)
        free(out);
    if (in)
        free(in);
    fprintf(stderr, "Failed to compress: %s\n", argv[1]);
    exit(1);
}