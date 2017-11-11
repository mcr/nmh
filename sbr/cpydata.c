/* cpydata.c -- copy all data from one fd to another
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "cpydata.h"
#include "error.h"

void
cpydata (int in, int out, const char *ifile, const char *ofile)
{
    int i;
    char buffer[BUFSIZ];

    while ((i = read(in, buffer, sizeof(buffer))) > 0) {
	if (write(out, buffer, i) != i)
	    adios(ofile, "error writing");
    }

    if (i == -1)
	adios(ifile, "error reading");
}
