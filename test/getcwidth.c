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

	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		fprintf(stderr, "Returns the column width of a UTF-8 "
			"multibyte character\n");
		exit(1);
	}

#ifndef MULTIBYTE_SUPPORT
	fprintf(stderr, "Nmh was not configured with multibyte support\n");
	exit(1);
#else
	/*
	 * It's not clear to me that we can just call mbtowc() with a
	 * combining character; just to be safe, feed it in a base
	 * character first.
	 */

	mbtowc(NULL, NULL, 0);

	charlen = mbtowc(&c, string, strlen(string));

	if (charlen != 1) {
		fprintf(stderr, "We expected a beginning character length "
			"of 1, got %d instead\n", charlen);
		exit(1);
	}

	p = string + charlen;

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
