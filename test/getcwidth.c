/*
 * getcwidth - Get the OS's idea of the width of Unicode codepoints
 *
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
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

#ifdef MULTIBYTE_SUPPORT
static void usage(char *);
static void dumpwidth(void);
static void getwidth(const char *);
#endif /* MULTIBYTE_SUPPORT */

int
main(int argc, char *argv[])
{
#ifndef MULTIBYTE_SUPPORT
	(void) argc;
	(void) argv;
	fprintf(stderr, "Nmh was not configured with multibyte support\n");
	exit(1);
#else /* MULTIBYTE_SUPPORT */
	wchar_t c;
	int i;

	setlocale(LC_ALL, "");

	if (argc < 2)
		usage(argv[0]);

	if (strcmp(argv[1], "--dump") == 0) {
		if (argc == 2) {
			dumpwidth();
			exit(0);
		} else {
			fprintf(stderr, "--dump cannot be combined with "
				"other arguments\n");
			exit(1);
		}
	}

	/*
	 * Process each argument.  If it begins with "U+", then try to
	 * convert it to a Unicode codepoint.  Otherwise, take each
	 * string and get the total width
	 */

	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "U+", 2) == 0) {
			/*
			 * We're making a big assumption here that
			 * wchar_t represents a Unicode codepoint.
			 * That technically isn't valid unless the
			 * C compiler defines __STDC_ISO_10646__, but
			 * we're going to assume now that it works.
			 */
			errno = 0;
			c = strtoul(argv[i] + 2, NULL, 16);
			if (errno) {
				fprintf(stderr, "Codepoint %s invalid\n",
					argv[i]);
				continue;
			}
			printf("%d\n", wcwidth(c));
		} else {
			getwidth(argv[i]);
		}
	}

	exit(0);
}

static void
usage(char *argv0)
{
	fprintf(stderr, "Usage: %s [--dump]\n", argv0);
	fprintf(stderr, "       %s U+XXXX [...]\n", argv0);
	fprintf(stderr, "       %s utf-8-sequence [...]\n", argv0);
	fprintf(stderr, "Returns the column width of a Unicode codepoint "
		"or UTF-8 character sequence\n");
	fprintf(stderr, "\t--dump\tDump complete width table\n");

	exit(1);
}

static void
getwidth(const char *string)
{
	wchar_t c;
	int charlen, charleft = strlen(string);
	int length = 0;

	/*
	 * In theory we should be able to use wcswidth(), but since we're
	 * testing out how the format libraries behave we'll do it a character
	 * at a time.
	 */

	mbtowc(NULL, NULL, 0);

	while (charleft > 0) {
		int clen;

		charlen = mbtowc(&c, string, charleft);

		if (charlen == 0)
			break;

		if (charlen < 0) {
			fprintf(stderr, "Unable to convert string \"%s\"\n",
				string);
			return;
		}

		if ((clen = wcwidth(c)) < 0) {
			fprintf(stderr, "U+%04lX non-printable\n",
				(unsigned long int) c);
			return;
		}

		length += clen;
		string += charlen;
		charleft -= charlen;
	}

	printf("%d\n", length);
}

static void
dumpwidth(void)
{
	wchar_t wc, low;
	int width, lastwidth;

	for (wc = 0, low = 1, lastwidth = wcwidth(1); wc < 0xffff; wc++) {
		width = wcwidth(wc+1);
		if (width != lastwidth) {
			printf("%04lX - %04lX = %d\n", (unsigned long int) low,
			       (unsigned long int) (wc), lastwidth);
			low = wc+1;
		}
		lastwidth = width;
	}

	width = wcwidth(wc);
	if (width == lastwidth)
		printf("%04lX - %04lX = %d\n", (unsigned long int) low,
		       (unsigned long int) (wc), width);
#endif /* MULTIBYTE_SUPPORT */
}
