/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

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
    if (!fname) {
        fprintf (stderr, "seahorse-xloadimage: specify output file in $SEAHORSE_IMAGE_FILE\n");
        exit (2);
    }

    f = fopen (fname, "wb");
    if (!f) {
        fprintf (stderr, "seahorse-xloadimage: couldn't open output file: %s", fname);
        exit (1);
    }

    do {
        len = fread (buf, sizeof(unsigned char), BUF_SIZE, stdin);
    } while (fwrite (buf, sizeof(unsigned char), len, f) == BUF_SIZE);

    if (ferror (f)) {
        fprintf (stderr, "seahorse-xloadimage: couldn't write to output file: %s", fname);
        exit (1);
    }
        
    fclose(f);
    return 0;
}
