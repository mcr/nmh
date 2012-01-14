
/*
 * fmt_scan.c -- format string interpretation
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 *
 * This is the engine that processes the format instructions created by
 * fmt_compile (found in fmt_compile.c).
 */

#include <h/mh.h>
#include <h/addrsbr.h>
#include <h/fmt_scan.h>
#include <h/tws.h>
#include <h/fmt_compile.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>
#ifdef MULTIBYTE_SUPPORT
#  include <wctype.h>
#  include <wchar.h>
#endif

extern char *formataddr ();	/* hook for custom address formatting */
extern char *concataddr ();	/* address formatting but allowing duplicates */

#ifdef LBL
struct msgs *fmt_current_folder; /* current folder (set by main program) */
#endif

extern int fmt_norm;		/* defined in sbr/fmt_def.c = AD_NAME */
struct mailname fmt_mnull;

/*
 * static prototypes
 */
static int match (char *, char *);
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

#ifdef LOCALE
    while ((c1 = *sub)) {
	c1 = (isalpha(c1) && isupper(c1)) ? tolower(c1) : c1;
	while ((c2 = *str++) && c1 != ((isalpha(c2) && isupper(c2)) ? tolower(c2) : c2))
	    ;
	if (! c2)
	    return 0;
	s1 = sub + 1; s2 = str;
	while ((c1 = *s1++) && ((isalpha(c1) && isupper(c1)) ? tolower(c1) : c1) == ((isalpha(c2 =*s2++) && isupper(c2)) ? tolower(c2) : c2))
	    ;
	if (! c1)
	    return 1;
    }
#else
    while ((c1 = *sub)) {
	while ((c2 = *str++) && (c1 | 040) != (c2 | 040))
	    ;
	if (! c2)
	    return 0;
	s1 = sub + 1; s2 = str;
	while ((c1 = *s1++) && (c1 | 040) == (*s2++ | 040))
	    ;
	if (! c1)
	    return 1;
    }
#endif
    return 1;
}

/*
 * copy a number to the destination subject to a maximum width
 */
static void
cpnumber(char **dest, int num, unsigned int wid, char fill, size_t n) {
    int i, c;
    char *sp;
    char *cp = *dest;
    char *ep = cp + n;

    if (cp + wid < ep) {
	if ((i = (num)) < 0)
	    i = -(num);
	if ((c = (wid)) < 0)
	    c = -c;
	sp = cp + c;
	do {
	    *--sp = (i % 10) + '0';
	    i /= 10;
	} while (i > 0 && sp > cp);
	if (i > 0)
	    *sp = '?';
	else if ((num) < 0 && sp > cp)
	    *--sp = '-';
	while (sp > cp)
	    *--sp = fill;
	cp += c;
    }
    *dest = cp;
}

/*
 * copy string from str to dest padding with the fill character to a size
 * of wid characters. if wid is negative, the string is right aligned
 * no more than n bytes are copied
 */
static void
cptrimmed(char **dest, char *str, unsigned int wid, char fill, size_t n) {
    int remaining;     /* remaining output width available */
    int c, ljust;
    int end;           /* number of input bytes remaining in str */
#ifdef MULTIBYTE_SUPPORT
    int char_len;      /* bytes in current character */
    int w;
    wchar_t wide_char;
#endif
    char *sp;          /* current position in source string */
    char *cp = *dest;  /* current position in destination string */
    char *ep = cp + n; /* end of destination buffer */
    int prevCtrl = 1;

    /* get alignment */
    ljust = 0;
    if ((remaining = (wid)) < 0) {
	remaining = -remaining;
	ljust++;
    }
    if ((sp = (str))) {
	mbtowc(NULL, NULL, 0); /* reset shift state */
	end = strlen(str);
	while (*sp && remaining > 0 && end > 0) {
#ifdef MULTIBYTE_SUPPORT
	    char_len = mbtowc(&wide_char, sp, end);
	    if (char_len <= 0 || (cp + char_len > ep))
		break;

	    end -= char_len;

	    if (iswcntrl(wide_char) || iswspace(wide_char)) {
		sp += char_len;
#else
	    end--;
	    if (iscntrl(*sp) || isspace(*sp)) {
		sp++;
#endif
		if (!prevCtrl) {
		    *cp++ = ' ';
		    remaining--;
		}

		prevCtrl = 1;
		continue;
	    }
	    prevCtrl = 0;

#ifdef MULTIBYTE_SUPPORT
	    w = wcwidth(wide_char);
	    if (w >= 0 && remaining >= w) {
		strncpy(cp, sp, char_len);
		cp += char_len;
		remaining -= w;
	    }
	    sp += char_len;
#else
	    *cp++ = *sp++;
	    remaining--;
#endif
	}
    }

    if (ljust) {
	if (cp + remaining > ep)
	    remaining = ep - cp;
	ep = cp + remaining;
	if (remaining > 0) {
	    /* copy string to the right */
	    while (--cp >= *dest)
		*(cp + remaining) = *cp;
	    /* add padding at the beginning */
	    cp += remaining;
	    for (c=remaining; c>0; c--)
		*cp-- = fill;
	}
	*dest = ep;
    } else {
	/* pad remaining space */
	while (remaining-- > 0 && cp < ep)
		*cp++ = fill;
	*dest = cp;
    }
}

static void
cpstripped (char **start, char *end, char *str)
{
    int c;
    char *s = str;

    if (!s)
	return;

    /* skip any initial control characters or spaces */
    while ((c = (unsigned char) *s) &&
#ifdef LOCALE
	    (iscntrl(c) || isspace(c)))
#else
	    (c <= 32))
#endif
	s++;

    /* compact repeated control characters and spaces into a single space */
    while((c = (unsigned char) *s++) && *start < end)
	if (!iscntrl(c) && !isspace(c))
	    *(*start)++ = c;
	else {
	    while ((c = (unsigned char) *s) &&
#ifdef LOCALE
		    (iscntrl(c) || isspace(c)))
#else
		    (c <= 32))
#endif
		s++;
	    *(*start)++ = ' ';
	}
}

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
fmt_scan (struct format *format, char *scanl, int width, int *dat)
{
    char *cp, *ep;
    unsigned char *sp;
    char *savestr = NULL;
    unsigned char *str = NULL;
    char buffer[BUFSIZ], buffer2[BUFSIZ];
    int i, c, ljust, n;
    int value = 0;
    time_t t;
    struct format *fmt;
    struct comp *comp;
    struct tws *tws;
    struct mailname *mn;

    cp = scanl;
    ep = scanl + width - 1;

    for (fmt = format; fmt->f_type != FT_DONE; fmt++)
	switch (fmt->f_type) {
	case FT_PARSEADDR:
	case FT_PARSEDATE:
	    fmt->f_comp->c_flags &= ~CF_PARSED;
	    break;
	}

    fmt = format;

    while (cp < ep) {
	switch (fmt->f_type) {

	case FT_COMP:
	    cpstripped (&cp, ep, fmt->f_comp->c_text);
	    break;
	case FT_COMPF:
	    cptrimmed (&cp, fmt->f_comp->c_text, fmt->f_width, fmt->f_fill, ep - cp);
	    break;

	case FT_LIT:
	    sp = fmt->f_text;
	    while( (c = *sp++) && cp < ep)
		*cp++ = c;
	    break;
	case FT_LITF:
	    sp = fmt->f_text;
	    ljust = 0;
	    i = fmt->f_width;
	    if (i < 0) {
		i = -i;
		ljust++;		/* XXX should do something with this */
	    }
	    while( (c = *sp++) && --i >= 0 && cp < ep)
		*cp++ = c;
	    while( --i >= 0 && cp < ep)
		*cp++ = fmt->f_fill;
	    break;

	case FT_STR:
	    cpstripped (&cp, ep, str);
	    break;
	case FT_STRF:
	    cptrimmed (&cp, str, fmt->f_width, fmt->f_fill, ep - cp);
	    break;
	case FT_STRLIT:
	    sp = str;
	    while ((c = *sp++) && cp < ep)
		*cp++ = c;
	    break;
	case FT_STRFW:
	    adios (NULL, "internal error (FT_STRFW)");

	case FT_NUM:
	    n = snprintf(cp, ep - cp + 1, "%d", value);
	    if (n >= 0) {
		if (n >= ep - cp) {
		    cp = ep;
		} else
		    cp += n;
	    }
	    break;
	case FT_NUMF:
	    cpnumber (&cp, value, fmt->f_width, fmt->f_fill, ep - cp);
	    break;

	case FT_CHAR:
	    *cp++ = fmt->f_char;
	    break;

	case FT_DONE:
	    goto finished;

	case FT_IF_S:
	    if (!(value = (str && *str))) {
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_S_NULL:
	    if (!(value = (str == NULL || *str == 0))) {
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_V_EQ:
	    if (value != fmt->f_value) {
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_V_NE:
	    if (value == fmt->f_value) {
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_V_GT:
	    if (value <= fmt->f_value) {
		fmt += fmt->f_skip;
		continue;
	    }
	    break;

	case FT_IF_MATCH:
	    if (!(value = (str && match (str, fmt->f_text)))) {
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
		    unsigned char *xp;

		    strncpy(buffer, str, sizeof(buffer));
		    buffer[sizeof(buffer)-1] = '\0';
		    str = buffer;
		    while (isspace(*str))
			    str++;
		    ljust = 0;
		    if ((i = fmt->f_width) < 0) {
			    i = -i;
			    ljust++;
		    }

		    if (!ljust && i > 0 && (int) strlen(str) > i)
			    str[i] = '\0';
		    xp = str;
		    xp += strlen(str) - 1;
		    while (xp > str && isspace(*xp))
			    *xp-- = '\0';
		    if (ljust && i > 0 && (int) strlen(str) > i)
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
	    value = width - (cp - scanl);
	    break;
	case FT_LV_PLUS_L:
	    value += fmt->f_value;
	    break;
	case FT_LV_MINUS_L:
	    value = fmt->f_value - value;
	    break;
	case FT_LV_DIVIDE_L:
	    if (fmt->f_value)
		value = value / fmt->f_value;
	    else
		value = 0;
	    break;
	case FT_LV_MODULO_L:
	    if (fmt->f_value)
		value = value % fmt->f_value;
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
	    value = time((time_t *) 0) - value;
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
	case FT_LV_ZONEF:
	    if ((fmt->f_comp->c_tws->tw_flags & TW_SZONE) == TW_SZEXP)
		    value = 1;
	    else
		    value = -1;
	    break;
	case FT_LV_DST:
	    value = fmt->f_comp->c_tws->tw_flags & TW_DST;
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
	unfriendly: ;
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
		int m;
		strncpy(buffer, str, sizeof(buffer));
		/* strncpy doesn't NUL-terminate if it fills the buffer */
		buffer[sizeof(buffer)-1] = '\0';
		str = buffer;
	
		/* we will parse from buffer to buffer2 */
		n = 0; /* n is the input position in str */
		m = 0; /* m is the ouput position in buffer2 */

		while ( str[n] != '\0') {
		    switch ( str[n] ) {
			case '\\':
			    n++;
			    if ( str[n] != '\0')
				buffer2[m++] = str[n++];
			    break;
			case '"':
			    n++;
			    break;
			default:
			    buffer2[m++] = str[n++];
			    break;
			}
		}
		buffer2[m] = '\0';
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
		memset ((char *) comp->c_tws, 0, sizeof *comp->c_tws);
		comp->c_flags = CF_TRUE;
	    }
	    comp->c_flags |= CF_PARSED;
	    break;

	case FT_FORMATADDR:
	    /* hook for custom address list formatting (see replsbr.c) */
	    str = formataddr (savestr, str);
	    break;

	case FT_CONCATADDR:
	    /* The same as formataddr, but doesn't do duplicate suppression */
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
	    unsigned char *lp;
	    char *lastb;
	    int indent, wid, len;

	    lp = str;
	    wid = value;
	    len = strlen (str);
	    sp = fmt->f_text;
	    indent = strlen (sp);
	    wid -= indent;
	    while( (c = *sp++) && cp < ep)
		*cp++ = c;
	    while (len > wid) {
		/* try to break at a comma; failing that, break at a
		 * space.
		 */
		lastb = 0; sp = lp + wid;
		while (sp > lp && (c = *--sp) != ',') {
		    if (! lastb && isspace(c))
			lastb = sp - 1;
		}
		if (sp == lp) {
		    if (! (sp = lastb)) {
			sp = lp + wid - 1;
			while (*sp && *sp != ',' && !isspace(*sp))
			    sp++;
			if (*sp != ',')
			    sp--;
		    }
		}
		len -= sp - lp + 1;
		while (cp < ep && lp <= sp)
		    *cp++ = *lp++;
		while (isspace(*lp))
		    lp++, len--;
		if (*lp) {
		    if (cp < ep)
			*cp++ = '\n';
		    for (i=indent; cp < ep && i > 0; i--)
			*cp++ = ' ';
		}
	    }
	    cpstripped (&cp, ep, lp);
	    }
	    break;

	case FT_PARSEADDR:
	    comp = fmt->f_comp;
	    if (comp->c_flags & CF_PARSED)
		break;
	    if (comp->c_mn != &fmt_mnull)
		mnfree (comp->c_mn);
	    if ((sp = comp->c_text) && (sp = getname(sp)) &&
		(mn = getm (sp, NULL, 0, fmt_norm, NULL))) {
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
	    /*
	     * if there's no component, we say true.  Otherwise we
	     * say "true" only if we can parse the address and it
	     * matches one of our addresses.
	     */
	    comp = fmt->f_comp;
	    if (comp->c_mn != &fmt_mnull)
		mnfree (comp->c_mn);
	    if ((sp = comp->c_text) && (sp = getname(sp)) &&
		(mn = getm (sp, NULL, 0, AD_NAME, NULL))) {
		comp->c_mn = mn;
		if (ismymbox(mn))
		    comp->c_flags |= CF_TRUE;
		else
		    comp->c_flags &= ~CF_TRUE;
		while ((sp = getname(sp)))
		    if ((comp->c_flags & CF_TRUE) == 0 &&
			(mn = getm (sp, NULL, 0, AD_NAME, NULL)))
			if (ismymbox(mn))
			    comp->c_flags |= CF_TRUE;
	    } else {
		while (getname(""))		/* XXX */
		    ;
		if (comp->c_text == 0)
		    comp->c_flags |= CF_TRUE;
		else
		    comp->c_flags &= ~CF_TRUE;
		comp->c_mn = &fmt_mnull;
	    }
	    break;

	case FT_ADDTOSEQ:
#ifdef LBL
	    /* If we're working on a folder (as opposed to a file), add the
	     * current msg to sequence given in literal field.  Don't
	     * disturb string or value registers.
	     */
	    if (fmt_current_folder)
		    seq_addmsg(fmt_current_folder, fmt->f_text, dat[0], -1);
#endif
	    break;
	}
	fmt++;
    }
#ifndef JLR
    finished:;
    if (cp[-1] != '\n')
	*cp++ = '\n';
    *cp   = 0;
    return ((struct format *)0);
#else /* JLR */
    if (cp[-1] != '\n')
	*cp++ = '\n';
    while (fmt->f_type != FT_DONE)
	fmt++;

    finished:;
    *cp = '\0';
    return (fmt->f_value ? ++fmt : (struct format *) 0);

#endif /* JLR */
}
