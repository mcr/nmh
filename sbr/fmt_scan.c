/* fmt_scan.c -- format string interpretation
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 *
 * This is the engine that processes the format instructions created by
 * fmt_compile (found in fmt_compile.c).
 */

#include "h/mh.h"
#include "fmt_rfc2047.h"
#include "uprf.h"
#include "context_find.h"
#include "error.h"
#include "h/addrsbr.h"
#include "h/fmt_scan.h"
#include "h/tws.h"
#include "h/fmt_compile.h"
#include "h/utils.h"
#include "unquote.h"

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>
#ifdef MULTIBYTE_SUPPORT
#  include <wctype.h>
#  include <wchar.h>
#endif

struct mailname fmt_mnull = { NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0,
			      NULL, NULL };

/*
 * static prototypes
 */
static int match (char *, char *) PURE;
static char *get_x400_friendly (char *, char *, int);
static int get_x400_comp (char *, char *, char *, int);


/*
 * test if string "sub" appears anywhere in
 * string "str" (case insensitive).
 */

static int
match (char *str, char *sub)
{
    int c1, c2;
    char *s1, *s2;

    while ((c1 = *sub)) {
	c1 = tolower((unsigned char)c1);
	while ((c2 = *str++) && c1 != tolower((unsigned char)c2))
	    ;
	if (! c2)
	    return 0;
	s1 = sub + 1; s2 = str;
	while ((c1 = *s1++) &&
            tolower((unsigned char)c1) == tolower((unsigned char)(c2 = *s2++)))
	    ;
	if (! c1)
	    return 1;
    }
    return 1;
}

/*
 * copy a number to the destination subject to a maximum width
 */
void
cpnumber(charstring_t dest, int num, int wid, char fill, size_t max)
{
    /* Maybe we should handle left padding at some point? */
    if (wid == 0)
	return;
    if (wid < 0)
        wid = -wid; /* OK because wid originally a short. */
    if ((size_t)wid < (num >= 0 ? max : max-1)) {
	/* Build up the string representation of num in reverse. */
	charstring_t rev = charstring_create (0);
	int i = num >= 0 ? num : -num;

	do {
	    charstring_push_back (rev, i % 10  +  '0');
	    i /= 10;
	} while (--wid > 0  &&  i > 0);
	if (i > 0) {
	    /* Overflowed the field (wid). */
	    charstring_push_back (rev, '?');
	} else if (num < 0  &&  wid > 0) {
	    /* Shouldn't need the wid > 0 check, that's why the condition
	       at the top checks wid < max-1 when num < 0. */
	    --wid;
	    if (fill == ' ') {
		charstring_push_back (rev, '-');
	    }
	}
	while (wid-- > 0  &&  fill != 0) {
	    charstring_push_back (rev, fill);
	}
	if (num < 0  &&  fill == '0') {
	    charstring_push_back (rev, '-');
	}

	{
	    /* Output the string in reverse. */
	    size_t b = charstring_bytes (rev);
	    const char *cp = b  ?  &charstring_buffer (rev)[b]  :  NULL;

	    for (; b > 0; --b) {
		charstring_push_back (dest, *--cp);
	    }
	}

	charstring_free (rev);
    }
}

/*
 * copy string from str to dest padding with the fill character to a
 * size of wid characters. if wid is negative, the string is right
 * aligned no more than max characters are copied
 */
void
cptrimmed(charstring_t dest, char *str, int wid, char fill, size_t max)
{
    int remaining;     /* remaining output width available */
    bool rjust;
    struct charstring *trimmed;
    size_t end;        /* number of input bytes remaining in str */
#ifdef MULTIBYTE_SUPPORT
    int char_len;      /* bytes in current character */
    int w;
    wchar_t wide_char;
    char *altstr = NULL;
#endif
    char *sp;          /* current position in source string */

    /* get alignment */
    rjust = false;
    if ((remaining = wid) < 0) {
	remaining = -remaining;
	rjust = true;
    }
    if (remaining > (int) max) { remaining = max; }

    trimmed = rjust ? charstring_create(remaining) : dest;

    bool prevCtrl = true;
    if ((sp = str)) {
#ifdef MULTIBYTE_SUPPORT
	if (mbtowc(NULL, NULL, 0)) {} /* reset shift state */
#endif
	end = strlen(str);
	while (*sp && remaining > 0 && end > 0) {
#ifdef MULTIBYTE_SUPPORT
	    char_len = mbtowc(&wide_char, sp, end);

	    /*
	     * See the relevant comments in cpstripped() to explain what's
	     * going on here; we want to handle the case where we get
	     * characters that mbtowc() cannot handle
	     */

	    if (char_len < 0) {
	    	altstr = "?";
		char_len = mbtowc(&wide_char, altstr, 1);
	    }

	    if (char_len <= 0) {
	    	break;
	    }

	    w = wcwidth(wide_char);

	    /* If w > remaining, w must be positive. */
	    if (w > remaining) {
		break;
	    }

	    end -= char_len;

	    if (iswcntrl(wide_char) || iswspace(wide_char)) {
		sp += char_len;
#else
            int c;
	    end--;
            /* isnctrl(), etc., take an int argument.  Cygwin's ctype.h
               intentionally warns if they are passed a char. */
            c = (unsigned char) *sp;
	    if (iscntrl(c) || isspace(c)) {
		sp++;
#endif
		if (!prevCtrl) {
                    charstring_push_back (trimmed, ' ');
		    remaining--;
		}

		prevCtrl = true;
		continue;
	    }
	    prevCtrl = false;

#ifdef MULTIBYTE_SUPPORT
	    if (w >= 0 && remaining >= w) {
                charstring_push_back_chars (trimmed, altstr ? altstr : sp,
					    char_len, w);
		remaining -= w;
		altstr = NULL;
	    }
	    sp += char_len;
#else
            charstring_push_back (trimmed, *sp++);
	    remaining--;
#endif
	}
    }

    while (remaining-- > 0) {
        charstring_push_back(dest, fill);
    }

    if (rjust) {
        charstring_append(dest, trimmed);
        charstring_free(trimmed);
    }
}

#ifdef MULTIBYTE_SUPPORT
static void
cpstripped (charstring_t dest, size_t max, char *str)
{
    static bool deja_vu;
    static char *oddchar;
    static size_t oddlen;
    static char *spacechar;
    static size_t spacelen;
    char *end;
    bool squash;
    char *src;
    int srclen;
    wchar_t rune;
    int w;

    if (!deja_vu) {
        size_t two;

        deja_vu = true;

        two = MB_CUR_MAX * 2; /* Varies at run-time. */

        oddchar = mh_xmalloc(two);
        oddlen = wcstombs(oddchar, L"?", two);
        assert(oddlen > 0);

        assert(wcwidth(L' ') == 1); /* Need to pad in ones. */
        spacechar = mh_xmalloc(two);
        spacelen = wcstombs(spacechar, L" ", two);
        assert(spacelen > 0);
    }

    if (!str)
        return; /* It's unclear why no padding in this case. */
    end = str + strlen(str);

    if (mbtowc(NULL, NULL, 0))
        {} /* Reset shift state. */

    squash = true; /* Trim `space' or `cntrl' from the start. */
    while (max) {
        if (!*str)
            return; /* It's unclear why no padding in this case. */

        srclen = mbtowc(&rune, str, end - str);
        if (srclen == -1) {
            /* Invalid rune, or not enough bytes to finish it. */
            rune = L'?';
            src = oddchar;
            srclen = oddlen;
            str++; /* Skip one byte. */
        } else {
            src = str;
            str += srclen;
        }

        if (iswspace(rune) || iswcntrl(rune)) {
            if (squash)
                continue; /* Amidst a run of these. */
            rune = L' ';
            src = spacechar;
            srclen = spacelen;
            squash = true;
        } else
            squash = false;

        w = wcwidth(rune);
        if (w == -1) {
            rune = L'?';
            w = wcwidth(rune);
            assert(w != -1);
            src = oddchar;
            srclen = oddlen;
        }

        if ((size_t)w > max) {
            /* No room for rune;  pad. */
            while (max--)
                charstring_push_back_chars(dest, spacechar, spacelen, 1);
            return;
        }

        charstring_push_back_chars(dest, src, srclen, w);
        max -= w;
    }
}
#endif

#ifndef MULTIBYTE_SUPPORT
static void
cpstripped (charstring_t dest, size_t max, char *str)
{
    bool squash;
    int c;

    if (!str)
	return;

    squash = true; /* Strip leading cases. */
    while (max--) {
        c = (unsigned char)*str++;
        if (!c)
            return;

	if (isspace(c) || iscntrl(c)) {
            if (squash)
                continue;
            c = ' ';
            squash = true;
        } else
            squash = false;

	charstring_push_back(dest, (char)c);
    }
}
#endif

static char *lmonth[] = { "January",  "February","March",   "April",
			  "May",      "June",    "July",    "August",
			  "September","October", "November","December" };

static char *
get_x400_friendly (char *mbox, char *buffer, int buffer_len)
{
    char given[BUFSIZ], surname[BUFSIZ];

    if (mbox == NULL)
	return NULL;
    if (*mbox == '"')
	mbox++;
    if (*mbox != '/')
	return NULL;

    if (get_x400_comp (mbox, "/PN=", buffer, buffer_len)) {
	for (mbox = buffer; (mbox = strchr(mbox, '.')); )
	    *mbox++ = ' ';

	return buffer;
    }

    if (!get_x400_comp (mbox, "/S=", surname, sizeof(surname)))
	return NULL;

    if (get_x400_comp (mbox, "/G=", given, sizeof(given)))
	snprintf (buffer, buffer_len, "%s %s", given, surname);
    else
	snprintf (buffer, buffer_len, "%s", surname);

    return buffer;
}

static int
get_x400_comp (char *mbox, char *key, char *buffer, int buffer_len)
{
    int	idx;
    char *cp;

    if ((idx = stringdex (key, mbox)) < 0
	    || !(cp = strchr(mbox += idx + strlen (key), '/')))
	return 0;

    snprintf (buffer, buffer_len, "%*.*s", (int)(cp - mbox), (int)(cp - mbox), mbox);
    return 1;
}

struct format *
fmt_scan (struct format *format, charstring_t scanlp, int width, int *dat,
	  struct fmt_callbacks *callbacks)
{
    char *sp;
    char *savestr, *str;
    char buffer[NMH_BUFSIZ], buffer2[NMH_BUFSIZ];
    int i, c;
    bool rjust;
    int value;
    time_t t;
    size_t max;
    struct format *fmt;
    struct comp *comp;
    struct tws *tws;
    struct mailname *mn;

    /*
     * max is the same as width, but unsigned so comparisons
     * with charstring_chars() won't raise compile warnings.
     */
    max = width;
    savestr = str = NULL;
    value = 0;

    for (fmt = format; fmt->f_type != FT_DONE; fmt++)
	switch (fmt->f_type) {
	case FT_PARSEADDR:
	case FT_PARSEDATE:
	    fmt->f_comp->c_flags &= ~CF_PARSED;
	    break;
	case FT_COMP:
	case FT_COMPF:
	case FT_LS_COMP:
	case FT_LS_DECODECOMP:
	    /*
	     * Trim these components of any newlines.
	     *
	     * But don't trim the "body" and "text" components.
	     */

	    comp = fmt->f_comp;

	    if (! (comp->c_flags & CF_TRIMMED) && comp->c_text &&
		(i = strlen(comp->c_text)) > 0) {
		if (comp->c_text[i - 1] == '\n' &&
		    strcmp(comp->c_name, "body") != 0 &&
		    strcmp(comp->c_name, "text") != 0)
		    comp->c_text[i - 1] = '\0';
		comp->c_flags |= CF_TRIMMED;
	    }
	    break;
	}

    fmt = format;

    for ( ; charstring_chars (scanlp) < max; ) {
	switch (fmt->f_type) {

	case FT_COMP:
	    cpstripped (scanlp, max - charstring_chars (scanlp),
                        fmt->f_comp->c_text);
	    break;
	case FT_COMPF:
	    cptrimmed (scanlp, fmt->f_comp->c_text, fmt->f_width,
		       fmt->f_fill, max - charstring_chars (scanlp));
	    break;

	case FT_LIT:
	    sp = fmt->f_text;
	    while ((c = *sp++) && charstring_chars (scanlp) < max) {
		charstring_push_back (scanlp, c);
	    }
	    break;
	case FT_LITF:
	    sp = fmt->f_text;
	    rjust = false;
	    i = fmt->f_width;
	    if (i < 0) {
		i = -i;
		rjust = true;		/* XXX should do something with this */
	    }
	    while ((c = *sp++) && --i >= 0 && charstring_chars (scanlp) < max) {
		charstring_push_back (scanlp, c);
	    }
	    while (--i >= 0 && charstring_chars (scanlp) < max) {
		charstring_push_back (scanlp, fmt->f_fill);
	    }
	    break;

	case FT_STR:
	    cpstripped (scanlp, max - charstring_chars (scanlp), str);
	    break;
	case FT_STRF:
	    cptrimmed (scanlp, str, fmt->f_width, fmt->f_fill,
		       max - charstring_chars (scanlp));
	    break;
	case FT_STRLIT:
	    if (str) {
		sp = str;
		while ((c = *sp++) && charstring_chars (scanlp) < max) {
		    charstring_push_back (scanlp, c);
		}
	    }
	    break;
	case FT_STRLITZ:
	    if (str) charstring_push_back_chars (scanlp, str, strlen (str), 0);
	    break;
	case FT_STRFW:
	    die("internal error (FT_STRFW)");

	case FT_NUM: {
	    int num = value;
	    unsigned int wid;

            for (wid = num <= 0; num; ++wid, num /= 10) {}
	    cpnumber (scanlp, value, wid, ' ',
		      max - charstring_chars (scanlp));
	    break;
        }
	case FT_LS_KILO:
	case FT_LS_KIBI:
	    {
		char *unitcp;
		unsigned int whole, tenths;
		unsigned int scale = 0;
		unsigned int val = (unsigned int)value;
		char *kibisuff = NULL;

		switch (fmt->f_type) {
		case FT_LS_KILO: scale = 1000; kibisuff = ""; break;
		case FT_LS_KIBI: scale = 1024; kibisuff = "i"; break;
		}

		if (val < scale) {
		    snprintf(buffer, sizeof(buffer), "%u", val);
		} else {
		    /* To prevent divide by 0, found by clang static
		       analyzer. */
		    if (scale == 0) { scale = 1; }

		    /* find correct scale for size (Kilo/Mega/Giga/Tera) */
		    for (unitcp = "KMGT"; val > (scale * scale); val /= scale) {
			if (!*++unitcp)
			    break;
		    }

		    if (!*unitcp) {
			strcpy(buffer, "huge");
		    } else {
			/* val is scale times too big.  we want tenths */
			val *= 10;

			/* round up */
			val += (scale - 1);
			val /= scale;

			whole = val / 10;
			tenths = val - (whole * 10);

			if (tenths) {
			    snprintf(buffer, sizeof(buffer), "%u.%u%c%s",
				    whole, tenths, *unitcp, kibisuff);
			} else {
			    snprintf(buffer, sizeof(buffer), "%u%c%s",
				    whole, *unitcp, kibisuff);
			}
		    }
		}
		str = buffer;
	    }
	    break;
	case FT_NUMF:
	    cpnumber (scanlp, value, fmt->f_width, fmt->f_fill,
		      max - charstring_chars (scanlp));
	    break;

	case FT_CHAR:
	    charstring_push_back (scanlp, fmt->f_char);
	    break;

	case FT_DONE:
	    if (callbacks && callbacks->trace_func)
		callbacks->trace_func(callbacks->trace_context, fmt, value,
				      str, charstring_buffer (scanlp));
	    goto finished;

	case FT_IF_S:
	    if (!(value = (str && *str))) {
		if (callbacks && callbacks->trace_func)
		    callbacks->trace_func(callbacks->trace_context, fmt, value,
					  str, charstring_buffer (scanlp));
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_S_NULL:
	    if (!(value = (str == NULL || *str == 0))) {
		if (callbacks && callbacks->trace_func)
		    callbacks->trace_func(callbacks->trace_context, fmt, value,
					  str, charstring_buffer (scanlp));
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_V_EQ:
	    if (value != fmt->f_value) {
		if (callbacks && callbacks->trace_func)
		    callbacks->trace_func(callbacks->trace_context, fmt, value,
					  str, charstring_buffer (scanlp));
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_V_NE:
	    if (value == fmt->f_value) {
		if (callbacks && callbacks->trace_func)
		    callbacks->trace_func(callbacks->trace_context, fmt, value,
					  str, charstring_buffer (scanlp));
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_V_GT:
	    if (value <= fmt->f_value) {
		if (callbacks && callbacks->trace_func)
		    callbacks->trace_func(callbacks->trace_context, fmt, value,
					  str, charstring_buffer (scanlp));
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_MATCH:
	    if (!(value = (str && match (str, fmt->f_text)))) {
		if (callbacks && callbacks->trace_func)
		    callbacks->trace_func(callbacks->trace_context, fmt, value,
					  str, charstring_buffer (scanlp));
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_V_MATCH:
	    if (str)
		value = match (str, fmt->f_text);
	    else
		value = 0;
	    break;

	case FT_IF_AMATCH:
	    if (!(value = (str && uprf (str, fmt->f_text)))) {
		if (callbacks && callbacks->trace_func)
		    callbacks->trace_func(callbacks->trace_context, fmt, value,
					  str, charstring_buffer (scanlp));
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_V_AMATCH:
	    value = uprf (str, fmt->f_text);
	    break;

	case FT_S_NONNULL:
	    value = (str != NULL && *str != 0);
	    break;

	case FT_S_NULL:
	    value = (str == NULL || *str == 0);
	    break;

	case FT_V_EQ:
	    value = (fmt->f_value == value);
	    break;

	case FT_V_NE:
	    value = (fmt->f_value != value);
	    break;

	case FT_V_GT:
	    value = (fmt->f_value > value);
	    break;

	case FT_GOTO:
	    if (callbacks && callbacks->trace_func)
		callbacks->trace_func(callbacks->trace_context, fmt, value,
				      str, charstring_buffer (scanlp));
	    fmt += fmt->f_skip;
	    continue;

	case FT_NOP:
	    break;

	case FT_LS_COMP:
	    str = fmt->f_comp->c_text;
	    break;
	case FT_LS_LIT:
	    str = fmt->f_text;
	    break;
	case FT_LS_GETENV:
	    if (!(str = getenv (fmt->f_text)))
		str = "";
	    break;
	case FT_LS_CFIND:
	    if (!(str = context_find (fmt->f_text)))
		str = "";
	    break;

	case FT_LS_DECODECOMP:
	    if (decode_rfc2047(fmt->f_comp->c_text, buffer2, sizeof(buffer2)))
		str = buffer2;
	    else
		str = fmt->f_comp->c_text;
	    break;

	case FT_LS_DECODE:
	    if (str && decode_rfc2047(str, buffer2, sizeof(buffer2)))
		str = buffer2;
	    break;

	case FT_LS_TRIM:
	    if (str) {
		    char *xp;

		    if (str != buffer)
			strncpy(buffer, str, sizeof(buffer));
		    buffer[sizeof(buffer)-1] = '\0';
		    str = buffer;
		    while (isspace((unsigned char) *str))
			    str++;
		    rjust = false;
		    if ((i = fmt->f_width) < 0) {
			    i = -i;
			    rjust = true;
		    }

		    if (!rjust && i > 0 && (int) strlen(str) > i)
			    str[i] = '\0';
		    xp = str;
		    xp += strlen(str) - 1;
		    while (xp > str && isspace((unsigned char) *xp))
			    *xp-- = '\0';
		    if (rjust && i > 0 && (int) strlen(str) > i)
			str += strlen(str) - i;
	    }
	    break;

	case FT_LV_COMPFLAG:
	    value = (fmt->f_comp->c_flags & CF_TRUE) != 0;
	    break;
	case FT_LV_COMP:
	    value = (comp = fmt->f_comp)->c_text ? atoi(comp->c_text) : 0;
	    break;
	case FT_LV_LIT:
	    value = fmt->f_value;
	    break;
	case FT_LV_DAT:
	    value = dat[fmt->f_value];
	    break;
	case FT_LV_STRLEN:
	    if (str != NULL)
		    value = strlen(str);
	    else
		    value = 0;
	    break;
	case FT_LV_CHAR_LEFT:
	    value = max - charstring_bytes (scanlp);
	    break;
	case FT_LV_PLUS_L:
	    value += fmt->f_value;
	    break;
	case FT_LV_MINUS_L:
	    value = fmt->f_value - value;
	    break;
	case FT_LV_MULTIPLY_L:
	    value *= fmt->f_value;
	    break;
	case FT_LV_DIVIDE_L:
	    if (fmt->f_value)
		value /= fmt->f_value;
	    else
		value = 0;
	    break;
	case FT_LV_MODULO_L:
	    if (fmt->f_value)
		value %= fmt->f_value;
	    else
		value = 0;
	    break;
	case FT_SAVESTR:
	    savestr = str;
	    break;

	case FT_LV_SEC:
	    value = fmt->f_comp->c_tws->tw_sec;
	    break;
	case FT_LV_MIN:
	    value = fmt->f_comp->c_tws->tw_min;
	    break;
	case FT_LV_HOUR:
	    value = fmt->f_comp->c_tws->tw_hour;
	    break;
	case FT_LV_MDAY:
	    value = fmt->f_comp->c_tws->tw_mday;
	    break;
	case FT_LV_MON:
	    value = fmt->f_comp->c_tws->tw_mon + 1;
	    break;
	case FT_LS_MONTH:
	    str = tw_moty[fmt->f_comp->c_tws->tw_mon];
	    break;
	case FT_LS_LMONTH:
	    str = lmonth[fmt->f_comp->c_tws->tw_mon];
	    break;
	case FT_LS_ZONE:
	    str = dtwszone (fmt->f_comp->c_tws);
	    break;
	case FT_LV_YEAR:
	    value = fmt->f_comp->c_tws->tw_year;
	    break;
	case FT_LV_WDAY:
	    if (!(((tws = fmt->f_comp->c_tws)->tw_flags) & (TW_SEXP|TW_SIMP)))
		set_dotw (tws);
	    value = tws->tw_wday;
	    break;
	case FT_LS_DAY:
	    if (!(((tws = fmt->f_comp->c_tws)->tw_flags) & (TW_SEXP|TW_SIMP)))
		set_dotw (tws);
	    str = tw_dotw[tws->tw_wday];
	    break;
	case FT_LS_WEEKDAY:
	    if (!(((tws = fmt->f_comp->c_tws)->tw_flags) & (TW_SEXP|TW_SIMP)))
		set_dotw (tws);
	    str = tw_ldotw[tws->tw_wday];
	    break;
	case FT_LV_YDAY:
	    value = fmt->f_comp->c_tws->tw_yday;
	    break;
	case FT_LV_ZONE:
	    value = fmt->f_comp->c_tws->tw_zone;
	    break;
	case FT_LV_CLOCK:
	    if ((value = fmt->f_comp->c_tws->tw_clock) == 0)
		value = dmktime(fmt->f_comp->c_tws);
	    break;
	case FT_LV_RCLOCK:
	    if ((value = fmt->f_comp->c_tws->tw_clock) == 0)
		value = dmktime(fmt->f_comp->c_tws);
	    value = time(NULL) - value;
	    break;
	case FT_LV_DAYF:
	    if (!(((tws = fmt->f_comp->c_tws)->tw_flags) & (TW_SEXP|TW_SIMP)))
		set_dotw (tws);
	    switch (fmt->f_comp->c_tws->tw_flags & TW_SDAY) {
		case TW_SEXP:
		    value = 1; break;
		case TW_SIMP:
		    value = 0; break;
		default:
		    value = -1; break;
	    }
	    break;
	case FT_LV_ZONEF:
	    if (fmt->f_comp->c_tws->tw_flags & TW_SZEXP)
		    value = 1;
	    else
		    value = -1;
	    break;
	case FT_LV_DST:
	    value = fmt->f_comp->c_tws->tw_flags & TW_DST ? 1 : 0;
	    break;
	case FT_LS_822DATE:
	    str = dasctime (fmt->f_comp->c_tws , TW_ZONE);
	    break;
	case FT_LS_PRETTY:
	    str = dasctime (fmt->f_comp->c_tws, TW_NULL);
	    break;

	case FT_LS_PERS:
	    str = fmt->f_comp->c_mn->m_pers;
	    break;
	case FT_LS_MBOX:
	    str = fmt->f_comp->c_mn->m_mbox;
	    break;
	case FT_LS_HOST:
	    str = fmt->f_comp->c_mn->m_host;
	    break;
	case FT_LS_PATH:
	    str = fmt->f_comp->c_mn->m_path;
	    break;
	case FT_LS_GNAME:
	    str = fmt->f_comp->c_mn->m_gname;
	    break;
	case FT_LS_NOTE:
	    str = fmt->f_comp->c_mn->m_note;
	    break;
	case FT_LS_822ADDR:
	    str = adrformat( fmt->f_comp->c_mn );
	    break;
	case FT_LV_HOSTTYPE:
	    value = fmt->f_comp->c_mn->m_type;
	    break;
	case FT_LV_INGRPF:
	    value = fmt->f_comp->c_mn->m_ingrp;
	    break;
	case FT_LV_NOHOSTF:
	    value = fmt->f_comp->c_mn->m_nohost;
	    break;
	case FT_LS_ADDR:
	case FT_LS_FRIENDLY:
	    if ((mn = fmt->f_comp->c_mn) == &fmt_mnull) {
		str = fmt->f_comp->c_text;
		break;
	    }
	    if (fmt->f_type == FT_LS_ADDR)
		goto unfriendly;
	    if ((str = mn->m_pers) == NULL) {
	        if ((str = mn->m_note)) {
		    if (str != buffer)
			strncpy (buffer, str, sizeof(buffer));
		    buffer[sizeof(buffer)-1] = '\0';
	            str = buffer;
	            if (*str == '(')
	            	str++;
	            sp = str + strlen(str) - 1;
	            if (*sp == ')') {
	            	*sp-- = '\0';
	        	while (sp >= str)
	            	    if (*sp == ' ')
	            		*sp-- = '\0';
	            	    else
	            		break;
	            }
	        } else if (!(str = get_x400_friendly (mn->m_mbox,
				buffer, sizeof(buffer)))) {
	unfriendly:
		  switch (mn->m_type) {
		    case LOCALHOST:
			str = mn->m_mbox;
			break;
		    case UUCPHOST:
			snprintf (buffer, sizeof(buffer), "%s!%s",
				mn->m_host, mn->m_mbox);
			str = buffer;
			break;
		    default:
			if (mn->m_mbox) {
			    snprintf (buffer, sizeof(buffer), "%s@%s",
				mn->m_mbox, mn->m_host);
			    str= buffer;
			}
			else
			    str = mn->m_text;
			break;
		  }
		}
	    }
	    break;


		/* UNQUOTEs RFC-2822 quoted-string and quoted-pair */
	case FT_LS_UNQUOTE:
	    if (str) {	  	
		if (str != buffer)
		    strncpy(buffer, str, sizeof(buffer));
		/* strncpy doesn't NUL-terminate if it fills the buffer */
		buffer[sizeof(buffer)-1] = '\0';
		unquote_string(buffer, buffer2);
		str = buffer2;
            }
	    break;

	case FT_LOCALDATE:
	    comp = fmt->f_comp;
	    if ((t = comp->c_tws->tw_clock) == 0)
		t = dmktime(comp->c_tws);
	    tws = dlocaltime(&t);
	    *comp->c_tws = *tws;
	    break;

	case FT_GMTDATE:
	    comp = fmt->f_comp;
	    if ((t = comp->c_tws->tw_clock) == 0)
		t = dmktime(comp->c_tws);
	    tws = dgmtime(&t);
	    *comp->c_tws = *tws;
	    break;

	case FT_PARSEDATE:
	    comp = fmt->f_comp;
	    if (comp->c_flags & CF_PARSED)
		break;
	    if ((sp = comp->c_text) && (tws = dparsetime(sp))) {
		*comp->c_tws = *tws;
		comp->c_flags &= ~CF_TRUE;
	    } else if ((comp->c_flags & CF_DATEFAB) == 0) {
		ZERO(comp->c_tws);
		comp->c_flags = CF_TRUE;
	    }
	    comp->c_flags |= CF_PARSED;
	    break;

	case FT_FORMATADDR:
	    /* hook for custom address list formatting (see replsbr.c) */
	    if (callbacks && callbacks->formataddr)
		str = callbacks->formataddr (savestr, str);
	    else
		str = formataddr (savestr, str);
	    break;

	case FT_CONCATADDR:
	    /* The same as formataddr, but doesn't do duplicate suppression */
	    if (callbacks && callbacks->concataddr)
		str = callbacks->concataddr (savestr, str);
	    else
		str = concataddr (savestr, str);
	    break;

	case FT_PUTADDR:
	    /* output the str register as an address component,
	     * splitting it into multiple lines if necessary.  The
	     * value reg. contains the max line length.  The lit.
	     * field may contain a string to prepend to the result
	     * (e.g., "To: ")
	     */
	    {
	    char *lp, *lastb;
	    int indent, wid, len;

	    lp = str;
	    wid = value;
	    len = str ? strlen (str) : 0;
	    sp = fmt->f_text;
	    indent = strlen (sp);
	    wid -= indent;
	    if (wid <= 0) {
	    	die("putaddr -- num register (%d) must be greater "
			    "than label width (%d)", value, indent);
	    }
	    while ((c = *sp++) && charstring_chars (scanlp) < max) {
		charstring_push_back (scanlp, c);
	    }
	    while (len > wid) {
		/* try to break at a comma; failing that, break at a
		 * space.
		 */
		lastb = 0; sp = lp + wid;
		while (sp > lp && (c = (unsigned char) *--sp) != ',') {
		    if (! lastb && isspace(c))
			lastb = sp - 1;
		}
		if (sp == lp) {
		    if (! (sp = lastb)) {
			sp = lp + wid - 1;
			while (*sp && *sp != ',' &&
						!isspace((unsigned char) *sp))
			    sp++;
			if (*sp != ',')
			    sp--;
		    }
		}
		len -= sp - lp + 1;
		while (lp <= sp && charstring_chars (scanlp) < max) {
		    charstring_push_back (scanlp, *lp++);
		}
		while (isspace((unsigned char) *lp))
		    lp++, len--;
		if (*lp) {
		    if (charstring_chars (scanlp) < max) {
			charstring_push_back (scanlp, '\n');
                    }
		    for (i=indent;
			 charstring_chars (scanlp) < max && i > 0;
			 i--)
			charstring_push_back (scanlp, ' ');
		}
	    }
	    cpstripped (scanlp, max - charstring_chars (scanlp), lp);
	    }
	    break;

	case FT_PARSEADDR:
	    comp = fmt->f_comp;
	    if (comp->c_flags & CF_PARSED)
		break;
	    if (comp->c_mn != &fmt_mnull)
		mnfree (comp->c_mn);
	    if ((sp = comp->c_text) && (sp = getname(sp)) &&
		(mn = getm (sp, NULL, 0, NULL, 0))) {
		comp->c_mn = mn;
		while (getname(""))
		    ;
		comp->c_flags |= CF_PARSED;
	    } else {
		while (getname(""))		/* XXX */
		    ;
		comp->c_mn = &fmt_mnull;
	    }
	    break;

	case FT_MYMBOX:
	case FT_GETMYMBOX:
	case FT_GETMYADDR:
	    /*
	     * if there's no component, we say true.  Otherwise we
	     * say "true" only if we can parse the address and it
	     * matches one of our addresses.
	     */
	    comp = fmt->f_comp;
	    if (comp->c_mn != &fmt_mnull)
		mnfree (comp->c_mn);
	    if ((sp = comp->c_text) && (sp = getname(sp)) &&
		(mn = getm (sp, NULL, 0, NULL, 0))) {
		comp->c_mn = mn;
		if (ismymbox(mn)) {
		    comp->c_flags |= CF_TRUE;
		    /* Set str for use with FT_GETMYMBOX.  With
		       FT_GETMYADDR, comp->c_mn will be run through
		       FT_LS_ADDR, which will strip off any pers
		       name. */
		    str = mn->m_text;
		} else {
		    comp->c_flags &= ~CF_TRUE;
		}
		while ((sp = getname(sp)))
		    if ((comp->c_flags & CF_TRUE) == 0 &&
			(mn = getm (sp, NULL, 0, NULL, 0)))
			if (ismymbox(mn)) {
			    comp->c_flags |= CF_TRUE;
			    /* Set str and comp->c_text for use with
			       FT_GETMYMBOX.  With FT_GETMYADDR,
			       comp->c_mn will be run through
			       FT_LS_ADDR, which will strip off any
			       pers name. */
			    free (comp->c_text);
			    comp->c_text = str = strdup (mn->m_text);
			    comp->c_mn = mn;
			}
		comp->c_flags |= CF_PARSED;
	    } else {
		while (getname(""))		/* XXX */
		    ;
		if (comp->c_text == 0)
		    comp->c_flags |= CF_TRUE;
		else {
		    comp->c_flags &= ~CF_TRUE;
		}
		comp->c_mn = &fmt_mnull;
	    }
	    if ((comp->c_flags & CF_TRUE) == 0  &&
		(fmt->f_type == FT_GETMYMBOX || fmt->f_type == FT_GETMYADDR)) {
		/* Fool FT_LS_ADDR into not producing an address. */
		comp->c_mn = &fmt_mnull; comp->c_text = NULL;
	    }
	    break;
	}

	/*
	 * Call our tracing callback function, if one was supplied
	 */

	if (callbacks && callbacks->trace_func)
	    callbacks->trace_func(callbacks->trace_context, fmt, value,
				  str, charstring_buffer (scanlp));
	fmt++;
    }

    /* Emit any trailing sequences of zero display length. */
    while (fmt->f_type != FT_DONE) {
	if (fmt->f_type == FT_LS_LIT) {
	    str = fmt->f_text;
	    if (callbacks && callbacks->trace_func)
		callbacks->trace_func(callbacks->trace_context, fmt, value,
				      str, charstring_buffer (scanlp));
	} else if (fmt->f_type == FT_STRLITZ) {
	    /* Don't want to emit part of an escape sequence.  So if
	       there isn't enough room in the buffer for the entire
	       string, skip it completely.  Need room for null
	       terminator, and maybe trailing newline (added below). */
	    if (str) {
		for (sp = str; *sp; ++sp) {
		    charstring_push_back (scanlp, *sp);
		}
	    }
	    if (callbacks && callbacks->trace_func)
		callbacks->trace_func(callbacks->trace_context, fmt, value,
				      str, charstring_buffer (scanlp));
	}
	fmt++;
    }

    finished:
    if (charstring_bytes (scanlp) > 0) {
	/*
	 * Append a newline if the last character wasn't.
	 */
#ifdef MULTIBYTE_SUPPORT
	/*
	 * It's a little tricky because the last byte might be part of
	 * a multibyte character, in which case we assume that wasn't
	 * a newline.
	 */
	size_t last_char_len = charstring_last_char_len (scanlp);
#else  /* ! MULTIBYTE_SUPPORT */
	size_t last_char_len = 1;
#endif /* ! MULTIBYTE_SUPPORT */

	if (last_char_len > 1  ||
	    charstring_buffer (scanlp)[charstring_bytes (scanlp) - 1] != '\n') {
	    charstring_push_back (scanlp, '\n');
	}
    }

    return NULL;
}
