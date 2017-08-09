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
#include <wctype.h>
#endif

#ifdef MULTIBYTE_SUPPORT
static void usage(char *);
static void dumpwidth(void);
static void dumpctype(void);
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

	if (! setlocale(LC_ALL, "")) {
		fprintf(stderr, "setlocale failed, check your LC_ALL, "
		    "LC_CTYPE, and LANG environment variables\n");
	}

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

	if (strcmp(argv[1], "--ctype") == 0) {
		if (argc != 2) {
			fprintf(stderr, "--ctype cannot be combined with other arguments\n");
			exit(1);
		}
		dumpctype();
		exit(0);
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
	fprintf(stderr, "       %s [--ctype]\n", argv0);
	fprintf(stderr, "       %s U+XXXX [...]\n", argv0);
	fprintf(stderr, "       %s utf-8-sequence [...]\n", argv0);
	fprintf(stderr, "Returns the column width of a Unicode codepoint "
		"or UTF-8 character sequence\n");
	fprintf(stderr, "\t--dump\tDump complete width table\n");
	fprintf(stderr, "\t--ctype\tPrint wctype(3) table.\n");

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

	if (mbtowc(NULL, NULL, 0)) {}

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

typedef struct {
	wchar_t min, max;
} unicode_range;

static unicode_range range[] = {
	/* https://en.wikipedia.org/wiki/Unicode#Code_point_planes_and_blocks */
	{  L'\x0000',    L'\xff' },
#if __WCHAR_MAX__ >= 0xffff
	{  L'\x0100',  L'\xffff' },
#if __WCHAR_MAX__ >= 0xfffff
	{ L'\x10000', L'\x14fff' },
	{ L'\x16000', L'\x18fff' },
	{ L'\x1b000', L'\x1bfff' },
	{ L'\x1d000', L'\x1ffff' },
	{ L'\x20000', L'\x2ffff' },
	{ L'\xe0000', L'\xe0fff' },
#endif
#endif
	{ L'\0', L'\0' }, /* Terminates list. */
};

static void
dumpwidth(void)
{
	unicode_range *r;
	int first;
	wchar_t wc, start;
	int width, lastwidth;

	for (r = range; r->max; r++) {
		first = 1;
		for (wc = r->min; wc <= r->max; wc++) {
			width = wcwidth(wc);
			if (first) {
				start = wc;
				lastwidth = width;
				first = 0;
				continue;
			}
			if (width != lastwidth) {
				printf("%04lX - %04lX = %d\n", (unsigned long)start,
					   (unsigned long int)wc - 1, lastwidth);
				start = wc;
				lastwidth = width;
			}
			if (wc == r->max) {
				printf("%04lX - %04lX = %d\n", (unsigned long)start,
					   (unsigned long int)wc, lastwidth);
                /* wchar_t can be a 16-bit unsigned short. */
                break;
            }
		}
	}
}

static void
dumpctype(void)
{
	unicode_range *r;
	wchar_t wc;

	for (r = range; r->max; r++) {
		for (wc = r->min; wc <= r->max; wc++) {
			printf("%6x  %2d  %c%c%c%c%c%c%c%c%c%c%c%c\n",
				wc, wcwidth(wc),
				iswcntrl(wc) ? 'c' : '-',
				iswprint(wc) ? 'p' : '-',
				iswgraph(wc) ? 'g' : '-',
				iswalpha(wc) ? 'a' : '-',
				iswupper(wc) ? 'u' : '-',
				iswlower(wc) ? 'l' : '-',
				iswdigit(wc) ? 'd' : '-',
				iswxdigit(wc) ? 'x' : '-',
				iswalnum(wc) ? 'N' : '-',
				iswpunct(wc) ? '@' : '-',
				iswspace(wc) ? 's' : '-',
				iswblank(wc) ? 'b' : '-');

            if (wc == r->max)
                /* wchar_t can be a 16-bit unsigned short. */
                break;
		}
	}
#endif /* MULTIBYTE_SUPPORT */
}
