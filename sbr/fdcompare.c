
/*
 * fdcompare.c -- are two files identical?
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
fdcompare (int fd1, int fd2)
{
    register int i, n1, n2, resp;
    register char *c1, *c2;
    char b1[BUFSIZ], b2[BUFSIZ];

    resp = 1;
    while ((n1 = read (fd1, b1, sizeof(b1))) >= 0
	    && (n2 = read (fd2, b2, sizeof(b2))) >= 0
	    && n1 == n2) {
	c1 = b1;
	c2 = b2;
	for (i = n1 < (int) sizeof(b1) ? n1 : (int) sizeof(b1); i--;)
	    if (*c1++ != *c2++) {
		resp = 0;
		goto leave;
	    }
	if (n1 < (int) sizeof(b1))
	    goto leave;
    }
    resp = 0;

leave: ;
    lseek (fd1, (off_t) 0, SEEK_SET);
    lseek (fd2, (off_t) 0, SEEK_SET);
    return resp;
}
