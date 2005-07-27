#include <stdio.h>
#include <stdlib.h>

#define BUF_SIZE 4096

int main (int argc, char* argv[])
{
    unsigned char buf[BUF_SIZE];
	const char* fname;
    size_t len;
    FILE* f;

    fname = getenv ("SEAHORSE_IMAGE_FILE");
    if (!fname)
        errx (2, "specify output file in $SEAHORSE_IMAGE_FILE");

    f = fopen (fname, "wb");
    if (!f)
        err (1, "couldn't open output file: %s", fname);

    do {
        len = fread (buf, sizeof(unsigned char), BUF_SIZE, stdin);
    } while (fwrite (buf, sizeof(unsigned char), len, f) == BUF_SIZE);

    if (ferror (f))
        err (1, "couldn't write to output file: %s", fname);

    fclose(f);
    return 0;
}
