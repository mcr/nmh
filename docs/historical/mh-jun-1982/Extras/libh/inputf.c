#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#
/* inputf is roughly complementary to printf,
except input comes from getchar (may be
user supplied, must return 0 at eof). in addition
all args must be pointers (addresses). the first arg
is the format string.  '%' is used as the escape char as in 
printf.  the general format is %w.dc where w and d are decimal
character counts, and c is one of (d, o, i, f, e, s, c, x).
the args are all assumed to be pointers to the appropriate
typed args -- note all floating point is assumed to be float
unless the %e format is used (extended float). if w is omitted
the field ends with the first field delimeter (typically, space, comma,
and newline -- these can be specified by following the specification
by a list of chars before the next '%'). d represents the default
number of decimal places for 'f' and 'e' if no explicit
decimal point.  for 's' w is the width of the field, and d is
the maximum number of chars after stripping off delimeters
on either end.  s and c are approximately identical, except
that s returns a pointer into a static buffer, and c returns char
by char into the program itself, which means a 'd' specification
for c is essential, and in fact 1 is assumed as a default (as opposed
to infinity for s). if a w field is specified, the input data
is assumed to be in a fixed format, with exactly w columns
devoted to the designated field.  this means that data in this format
does not have to be separated by blanks, etc.  chars in the format
string before the first '%' are output before any requests
to the subroutine for chars.  e.g.:
	inputf("-> %i%e%s\n",&integ, &dbl, &charptr);
would read an integer into integ, a double into dbl, and a string
terminated only by a <nl> into a static buffer, returning its start
addr in charptr, after prompting with "-> ". */
#define DFTSEP ",; \t\n\r"	/* default field separators */
#define NEXTCHR getchar()	/* character input routine */
#define SCBLEN 100		/* max. no. of chars in static char buff */
struct { float *fltp; };
struct { double *dblp; };
inputf(format,arg1,arg2,arg3)
char *format;
{
	register char *fp, *cp, cc;
	int **ap;
	int fld, val, wv, dv, radix;
	char formc, negflg, negexp, *fsp, *scp;
	static char scbuf[SCBLEN], fldsep[10];
	double dval;

	ap = &arg1;
	fp = format;
	scp = scbuf;	/* init static string pointer */

	while ((cc = *fp++) && cc != '%') putchar(cc);
	flush();

	fld = 0;	/* used as return value when error found */
	while (cc) {
		fld--;
		wv = 0; dv = 0;	/* init w.d fields */
		while (numeric(cc = *fp++)) wv = wv*10 + cc - '0';
		if (cc == '.')
			while (numeric(cc = *fp++)) dv = dv*10 + cc - '0';
		if (!wv) wv--; if (!dv) dv--;	/* zero --> minus one */
		formc = cc;	/* save format character */
		fsp = cp = fldsep;  /* collect terminators */
		while (*cp++ = *fp)
			if (*fp++ == '%') {
				fp--;
				*--cp = 0;
				break;
			}
		if (!*fsp) fsp = DFTSEP;
		switch (formc) {
		case 'o':
			radix = 8;
			goto ido;
		case 'd':
		case 'i':
			radix = 10;
		ido:
			val = 0;
			negflg = 0;
			while ((cc = NEXTCHR) && (cc == ' ' || cc == '-') && --wv)
				if (cc == '-') negflg++;
			if (!cc) return (fld);	/* return error if eof found */
			if (formc == 'i' && cc == '0') radix = 8;
			if (wv) do {
				if (numeric (cc)) {
					val = val*radix + cc - '0';
				} else if (any(cc,fsp))
					break;
			} while (--wv && (cc = NEXTCHR));
			if (negflg) val = -val;
			**ap++ = val;
			break;
		case 'c':
			if (dv < 0)
				if (wv < 0) dv = 1;
				else dv = wv;
			cp = *ap++;
			goto casecs;
		case 's':
			cp = scp;
			val = scbuf + SCBLEN - 1 - cp;	/* calc max. limit on input */
			if (dv<0 || dv>val)
				if ((dv = val) <= 0) return (fld);
		casecs:
			while ((cc = NEXTCHR) && cc == ' ' && --wv);
			if (!cc) return (fld);	/* return error if eof found */
			if (wv) do {
				if (any(cc,fsp)) break;
				if (!dv--) return (fld); /* too big */
				*cp++ = cc;
			} while (--wv && (cc = NEXTCHR));
			if (dv || formc == 's') *cp++ = 0;
			if (formc == 'c') break;
			**ap++ = scp;
			scp = cp;
			break;
		case 'x':
			while ((cc = NEXTCHR) && cc == ' ' && --wv);
			if (wv)
				while (!any(cc,fsp) && --wv && (cc = NEXTCHR));
			break;
		case 'f':
		case 'e':
			dval = 0.;
			negflg = 0;
			val = 0;	/* used for explicit exponent */
			radix = 0100000; /* used for decimal place count */
			while ((cc = NEXTCHR) && (cc == ' ' || cc == '-') && --wv)
				if (cc == '-') negflg++;
			if (!cc) return (fld);	/* return error if eof found */
			if (wv) do {
				if (numeric(cc)) {
					dval = dval*10. + (cc-'0');
					radix++;
				} else if (cc == '.') {
					radix = 0;
					dv = -1;
				} else if (any(cc,fsp)) break;
				else if (cc == 'e') {
					negexp = 0;
					while ((cc = NEXTCHR) && !numeric(cc) && --wv)
						if (cc == '-') negexp++;
					if (!cc) return (fld);	/* return error if eof found */
					if (wv) do {
						if (numeric(cc))
							val = val*10 + cc-'0';
						else if (any(cc,fsp)) break;
					} while (--wv && (cc = NEXTCHR));
					if (negexp) val = -val;
					break;
				}
			} while (--wv && (cc = NEXTCHR));
			if (dv > 0) radix = dv;
			if (radix > 0) val =- radix;
			if (val >= 0)
				while (--val >= 0) dval = dval*10.;
			else
				while (++val <= 0) dval = dval/10.;
			if (negflg) dval = -dval;
			if (formc == 'f')
				*ap->fltp = dval;
			else
				*ap->dblp = dval;
			ap++;
			break;
		default:
			write(2,"bad format specification",20);
			exit(fld);
		}
		cc = *fp++;
	}
	return (0);
}
numeric(chr)
{
	return (chr >= '0' && chr <= 