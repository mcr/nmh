
/*
 * cpydata.c -- copy all data from one fd to another
 *
 * $Id$
 */

#include <h/mh.h>

void
cpydata (int in, int out, char *ifile, char *ofile)
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
