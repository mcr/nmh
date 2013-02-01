/*
 * getcwidth - Get the OS's idea of the width of a combining character
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef MULTIBYTE_SUPPORT
#include <locale.h>
#include <wchar.h>
#endif

static void usage(char *);
static void dumpwidth(void);

int
main(int argc, char *argv[])
{
	wchar_t c;
	int charlen;
	char *p;

	/*
	 * This is the UTF-8 for "n" + U+0308 (Combining Diaeresis)
	 */

	unsigned char string[] = "n\xcc\x88";

	setlocale(LC_ALL, "");

	if (argc > 2)
		usage(argv[0]);

	if (argc == 2) {
		if (strcmp(argv[1], "--dump") == 0) {
			dumpwidth();
			exit(0);
		} else {
			usage(argv[0]);
		}
	}

#ifndef MULTIBYTE_SUPPORT
	fprintf(stderr, "Nmh was not configured with multibyte support\n");
	exit(1);
#else /* MULTIBYTE_SUPPORT */
	/*
	 * It's not clear to me that we can just call mbtowc() with a
	 * combining character; just to be safe, feed it in a base
	 * character first.
	 */

	mbtowc(NULL, NULL, 0);

	charlen = mbtowc(&c, (char *) string, strlen((char *) string));

	if (charlen != 1) {
		fprintf(stderr, "We expected a beginning character length "
			"of 1, got %d instead\n", charlen);
		exit(1);
	}

	p = (char *) (string + charlen);

	charlen = mbtowc(&c, p, strlen(p));

	if (charlen != 2) {
		fprintf(stderr, "We expected a multibyte character length "
			"of 2, got %d instead\n", charlen);
		fprintf(stderr, "Are you using a UTF-8 locale?\n");
		exit(1);
	}

	printf("%d\n", wcwidth(c));

	exit(0);
#endif /* MULTIBYTE_SUPPORT */
}

static void
usage(char *argv0)
{
	fprintf(stderr, "Usage: %s [--dump]\n", argv0);
	fprintf(stderr, "Returns the column width of a UTF-8 combining "
		"multibyte character\n");
	fprintf(stderr, "\t--dump\tDump complete width table\n");

	exit(1);
}

static void
dumpwidth(void)
{
#ifndef MULTIBYTE_SUPPORT
	fprintf(stderr, "Nmh was not configured with multibyte support\n");
	exit(1);
#else /* MULTIBYTE_SUPPORT */
	wchar_t wc, low;
	int width, lastwidth;

	for (wc = low = 1, lastwidth = wcwidth(wc); wc <= 0xffff; wc++) {
		width = wcwidth(wc);
		if (width != lastwidth) {
			printf("%04X - %04X = %d\n", low, wc - 1, lastwidth);
			low = wc;
		}
		lastwidth = width;
	}

	width = wcwidth(wc - 1);
	if (width == lastwidth)
		printf("%04X - %04X = %d\n", low, wc - 1, width);
#endif /* MULTIBYTE_SUPPORT */
}
