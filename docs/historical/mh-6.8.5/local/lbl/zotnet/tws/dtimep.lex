%{
#ifndef lint
static char rcsid[] =
#ifdef FLEX_SCANNER
    "@(#) $Header: dtimep.lex,v 1.15 90/10/16 14:57:20 leres Exp $ (LBL) (flex generated scanner)";
#else
    "@(#) $Header: dtimep.lex,v 1.15 90/10/16 14:57:20 leres Exp $ (LBL) (lex generated scanner)";
#endif
#endif
%}
/*
%e 2000
%p 5000
%n 1000
%a 4000
 */

%START	Z
sun	(sun(day)?)
mon	(mon(day)?)
tue	(tue(sday)?)
wed	(wed(nesday)?)
thu	(thu(rsday)?)
fri	(fri(day)?)
sat	(sat(urday)?)

DAY	({sun}|{mon}|{tue}|{wed}|{thu}|{fri}|{sat})

jan	(jan(uary)?)
feb	(feb(ruary)?)
mar	(mar(ch)?)
apr	(apr(il)?)
may	(may)
jun	(jun(e)?)
jul	(jul(y)?)
aug	(aug(ust)?)
sep	(sep(tember)?)
oct	(oct(ober)?)
nov	(nov(ember)?)
dec	(dec(ember)?)

MONTH	({jan}|{feb}|{mar}|{apr}|{may}|{jun}|{jul}|{aug}|{sep}|{oct}|{nov}|{dec})

w	([ \t]*)
W	([ \t]+)
D	([0-9]?[0-9])
d	[0-9]
%{
#include "tws.h"
#include <ctype.h>

/*
 * Patchable flag that says how to interpret NN/NN/NN dates. When
 * true, we do it European style: DD/MM/YY. When false, we do it
 * American style: MM/DD/YY.
 */
int europeandate = 0;

/*
 * Table to convert month names to numeric month.  We use the
 * fact that the low order 5 bits of the sum of the 2nd & 3rd
 * characters of the name is a hash with no collisions for the 12
 * valid month names.  (The mask to 5 bits maps any combination of
 * upper and lower case into the same hash value).
 */
static	int month_map[] = {
	0,
	6,	/* 1 - Jul */
	3,	/* 2 - Apr */
	5,	/* 3 - Jun */
	0,
	10,	/* 5 - Nov */
	0,
	1,	/* 7 - Feb */
	11,	/* 8 - Dec */
	0,
	0,
	0,
	0,
	0,
	0,
	0,	/*15 - Jan */
	0,
	0,
	0,
	2,	/*19 - Mar */
	0,
	8,	/*21 - Sep */
	0,
	9,	/*23 - Oct */
	0,
	0,
	4,	/*26 - May */
	0,
	7	/*28 - Aug */
};
/*
 * Same trick for day-of-week using the hash function
 *  (c1 & 7) + (c2 & 4)
 */
static	int day_map[] = {
	0,
	0,
	0,
	6,	/* 3 - Sat */
	4,	/* 4 - Thu */
	0,
	5,	/* 6 - Fri */
	0,	/* 7 - Sun */
	2,	/* 8 - Tue */
	1	/* 9 - Mon */,
	0,
	3	/*11 - Wed */
};
#define SETDAY	{ tw.tw_wday= day_map[(cp[0] & 7) + (cp[1] & 4)];\
		tw.tw_flags &= ~TW_SDAY; tw.tw_flags |= TW_SEXP;\
		cp += 2; }
#define SETMONTH { tw.tw_mon = month_map[(cp[0] + cp[1]) & 0x1f]; gotdate++;\
		 cp += 2;\
		 SKIPD;}
#define CVT2	(i=(*cp++ - '0'),isdigit(*cp)? i*10 + (*cp++ - '0') : i)
#define SKIPD	{ while ( !isdigit(*cp++) ) ;  --cp; }
#define EXPZONE	{ tw.tw_flags &= ~TW_SZONE; tw.tw_flags |= TW_SZEXP; }
#define ZONE(x)	{ tw.tw_zone=(x); EXPZONE; }
#define ZONED(x) { ZONE(x); tw.tw_flags |= TW_DST; }
#define	LC(c)	(isupper(c) ? tolower(c) : (c))

#ifdef FLEX_SCANNER
/* We get passed a string and return a pointer to a static tws struct */
#undef YY_DECL
#define YY_DECL struct tws *dparsetime(str) char *str;

/* We assume that we're never passed more than max_size characters */
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if (gstr) { \
		register char *xp; \
		xp = gstr; \
		while (isspace(*xp)) \
			xp++; \
		gstr = xp; \
		while (*xp != '\0') \
			++xp; \
		result = xp - gstr; \
		bcopy(gstr, buf, result); \
		gstr = 0; \
	} else \
		result = YY_NULL;

/* Date strings aren't usually very long */
#undef YY_READ_BUF_SIZE
#define YY_READ_BUF_SIZE 128

/* Use mh error reporting routine */
#undef YY_FATAL_ERROR
#define YY_FATAL_ERROR(s) adios("dparsetime()", s);

/* We need a pointer to the matched text we can modify */
#undef YY_USER_ACTION
#define YY_USER_ACTION cp = yytext

/* Used to initialize */
static struct tws ztw = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* Global for use by YY_INPUT() macro */
static char *gstr;
#endif

%}

%%
					register int i, gotdate;
					register char *cp;
					static struct tws tw;
					static void zonehack();

					BEGIN(INITIAL);
					yy_init = 1;
					tw = ztw;
					gstr = str;
					gotdate = 0;

{DAY}","?{w}				SETDAY;
"("{DAY}")"(","?)			{
					cp++;
					SETDAY;
					}

{D}"/"{D}"/"(19)?[0-9][0-9]{w}		{
					if (europeandate) {
						/* European: DD/MM/YY */
						tw.tw_mday = CVT2;
						cp++;
						tw.tw_mon  = CVT2 - 1;
					} else {
						/* American: MM/DD/YY */
						tw.tw_mon  = CVT2 - 1;
						cp++;
						tw.tw_mday = CVT2;
					}
					cp++;
					for (i = 0; isdigit(*cp); )
						i = i*10 + (*cp++ - '0');
					tw.tw_year = i % 100;
					gotdate++;
					}
{D}{w}(-)?{w}{MONTH}{w}(-)?{w}(19)?{D}{w}(\,{w}|at{W})? {
					tw.tw_mday = CVT2;
					while (!isalpha(*cp++))
						;
					SETMONTH;
					for (i = 0; isdigit(*cp); )
						i = i*10 + (*cp++ - '0');
					tw.tw_year = i % 100;
					}
{MONTH}{W}{D}","{W}(19)?{D}{w}		{
					cp++;
					SETMONTH;
					tw.tw_mday = CVT2;
					SKIPD;
					for (i = 0; isdigit(*cp); )
						i = i*10 + (*cp++ - '0');
					tw.tw_year = i % 100;
					}

{MONTH}{W}{D}{w}			{
					cp++;
					SETMONTH;
					tw.tw_mday = CVT2;
					}

{D}:{D}:{D}{w}				{
					tw.tw_hour = CVT2; cp++;
					tw.tw_min  = CVT2; cp++;
					tw.tw_sec  = CVT2;
					BEGIN Z;
					}
{D}:{D}{w}				{
					tw.tw_hour = CVT2; cp++;
					tw.tw_min  = CVT2;
					BEGIN Z;
					}
{D}:{D}{w}am{w}				{
					tw.tw_hour = CVT2; cp++;
					if (tw.tw_hour == 12)
						tw.tw_hour = 0;
					tw.tw_min  = CVT2;
					BEGIN Z;
					}
{D}:{D}{w}pm{w}				{
					tw.tw_hour = CVT2; cp++;
					if (tw.tw_hour != 12)
						tw.tw_hour += 12;
					tw.tw_min  = CVT2;
					BEGIN Z;
					}
[0-2]{d}{d}{d}{d}{d}{w}			{
					tw.tw_hour = CVT2;
					tw.tw_min  = CVT2;
					tw.tw_sec  = CVT2;
					BEGIN Z;
					}
19[6-9]{d}{w}				{
					/*
					 * Luckly, 4 digit times in the range
					 * 1960-1999 aren't legal as hour
					 * and minutes. This rule must come
					 * the 4 digit hour and minute rule.
					 */
					cp += 2;
					tw.tw_year = CVT2;
					}
[0-2]{d}{d}{d}{w}			{
					tw.tw_hour = CVT2;
					tw.tw_min  = CVT2;
					BEGIN Z;
					}
<Z>"-"?ut				ZONE(0 * 60);
<Z>"-"?gmt				ZONE(0 * 60);
<Z>"-"?jst				ZONE(2 * 60);
<Z>"-"?jdt				ZONED(2 * 60);
<Z>"-"?est				ZONE(-5 * 60);
<Z>"-"?edt				ZONED(-5 * 60);
<Z>"-"?cst				ZONE(-6 * 60);
<Z>"-"?cdt				ZONED(-6 * 60);
<Z>"-"?mst				ZONE(-7 * 60);
<Z>"-"?mdt				ZONED(-7 * 60);
<Z>"-"?pst				ZONE(-8 * 60);
<Z>"-"?pdt				ZONED(-8 * 60);
<Z>"-"?nst				ZONE(-(3 * 60 + 30));
<Z>"-"?ast				ZONE(-4 * 60);
<Z>"-"?adt				ZONED(-4 * 60);
<Z>"-"?yst				ZONE(-9 * 60);
<Z>"-"?ydt				ZONED(-9 * 60);
<Z>"-"?hst				ZONE(-10 * 60);
<Z>"-"?hdt				ZONED(-10 * 60);
<Z>"-"?bst				ZONED(-1 * 60);
<Z>[a-i]				{
					tw.tw_zone = 60 * (('a'-1) - LC(*cp));
					EXPZONE; 
					}
<Z>[k-m]				{
					tw.tw_zone = 60 * ('a' - LC(*cp));
					EXPZONE; 
					}
<Z>[n-y]				{
					tw.tw_zone = 60 * (LC(*cp) - 'm');
					EXPZONE; 
					}
<Z>"+"[0-1]{d}{d}{d}			{
					cp++;
					tw.tw_zone = ((cp[0] * 10 + cp[1])
						     -('0' * 10   + '0'))*60
						    +((cp[2] * 10 + cp[3])
						     -('0' * 10   + '0'));
#ifdef	DSTXXX
					zonehack (&tw);
#endif	DSTXXX
					cp += 4;
					EXPZONE; 
					}
<Z>"-"[0-1]{d}{d}{d}			{
					cp++;
					tw.tw_zone = (('0' * 10   + '0')
						     -(cp[0] * 10 + cp[1]))*60
						    +(('0' * 10   + '0')
						     -(cp[2] * 10 + cp[3]));
#ifdef	DSTXXX
					zonehack (&tw);
#endif	DSTXXX
					cp += 4;
					EXPZONE; 
					}

\n					|
{W}					;

<INITIAL,Z><<EOF>>			return(&tw);

.					{
					/*
					 * We jammed; it's an error if we
					 * didn't parse anything.
					 */
					if (!gotdate || tw.tw_year == 0)
						return(0);
					return(&tw);
					}

%%

#ifdef	DSTXXX
#include <sys/types.h>
#ifndef	BSD42
#include <time.h>
#else	BSD42
#include <sys/time.h>
#endif	BSD42

static void
zonehack(tw)
    register struct tws *tw;
{
    register struct tm *tm;

    if (twclock (tw) == -1L)
	return;

    tm = localtime (&tw -> tw_clock);
    if (tm -> tm_isdst) {
	tw -> tw_flags |= TW_DST;
	tw -> tw_zone -= 60;
    }
}
#endif	DSTXXX
