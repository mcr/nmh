%e 2000
%p 5000
%n 1000
%a 4000
%{
#include <h/nmh.h>
#include <h/tws.h>

  /* Since we're looking at a string at a time, don't worry about
   *  wrapping to the next buffer.
   */
#define yywrap() 1
#define YY_SKIP_YYWRAP

#define YY_NO_UNPUT

  /* This is the tricky thing that makes this function cool.  We
   *  replace the traditional int yylex(void) declaration with our
   *  dparsetime() declaration, essentially piggy-backing off the
   *  utility of the yylex() function and adding what we need to make
   *  the parsing function useful to us.
   */
#define YY_DECL struct tws *dparsetime(char *lexstr)

  /* yyerminate() is called after the input string is matched to
   * completion (actually, when the lexer reaches an EOF).  The only
   * thing that really needs to be in this macro function is the
   * return call, which must be substituted inline into dparsetime.
   */

#define yyterminate() (void)yy_delete_buffer(lexhandle); \
  if(!(tw.tw_flags & TW_SUCC)) { \
    return (struct tws *)NULL; \
  } \
  if(tw.tw_year < 1960) \
    tw.tw_year += 1900; \
  if(tw.tw_year < 1960) \
    tw.tw_year += 100; \
  return(&tw)

/*
 * Patchable flag that says how to interpret NN/NN/NN dates. When
 * true, we do it European style: DD/MM/YY. When false, we do it
 * American style: MM/DD/YY.  Of course, these are all non-RFC822
 * compliant.
 */
int europeandate = 0;

/*
 * Table to convert month names to numeric month.  We use the
 * fact that the low order 5 bits of the sum of the 2nd & 3rd
 * characters of the name is a hash with no collisions for the 12
 * valid month names.  (The mask to 5 bits maps any combination of
 * upper and lower case into the same hash value).
 */
static int month_map[] = {
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
 * Lookup table for day-of-week using the same hash trick as for above name-of-
 * month table, but using the first and second character, not second and third.
 *
 * Compute index into table using: (day_name[0] & 7) + (day_name[1] & 4)
 */
static int day_map[] = {
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

/* The SET* macros will parse for the appropriate field, and leave the
 * cp pointer at the first character after the desired field. Be
 * careful with variable-length fields or alpha-num mixes.

 * The SKIP* macros skip over characters of a particular class and
 * leave cp at the position of the first character that doesn't match
 * that class. Correspondingly, SKIPTO* skips until it reaches a
 * character of a particular class.
 */

#define INIT()       { cp = yytext;} 
#define SETWDAY()    { tw.tw_wday= day_map[(cp[0] & 7) + (cp[1] & 4)]; \
                       tw.tw_flags &= ~TW_SDAY; tw.tw_flags |= TW_SEXP; \
                       SKIPA(); }
#define SETMON()     { cp++; \
                       tw.tw_mon = month_map[(cp[0] + cp[1]) & 0x1f]; \
  		       SKIPA(); }
#define SETMON_NUM() { tw.tw_mon = atoi(cp)-1; \
  		       SKIPD(); }
#define SETYEAR()    { tw.tw_year = atoi(cp); \
  		       SKIPD(); }
#define SETDAY()     { tw.tw_mday = atoi(cp); \
                       tw.tw_flags |= TW_YES; \
  		       SKIPD(); }
#define SETTIME()    { tw.tw_hour = atoi(cp); \
                       cp += 2; \
                       SKIPTOD(); \
                       tw.tw_min = atoi(cp); \
                       cp += 2; \
                       if(*cp == ':') { \
                          tw.tw_sec = atoi(++cp); SKIPD(); } }
#define SETZONE(x)   { tw.tw_zone = ((x)/100)*60+(x)%100; \
                       tw.tw_flags |= TW_SZEXP; \
                       SKIPD(); }
#define SETDST()     { tw.tw_flags |= TW_DST; }
#define SKIPD()      { while ( isdigit(*cp++) ) ; \
                       --cp; }
#define SKIPTOD()    { while ( !isdigit(*cp++) ) ; \
                       --cp; }
#define SKIPA()      { while ( isalpha(*cp++) ) ; \
                       --cp; }
#define SKIPTOA()    { while ( !isalpha(*cp++) ) ; \
                       --cp; }
#define SKIPSP()     { while ( isspace(*cp++) ) ; \
                       --cp; }
#define SKIPTOSP()   { while ( !isspace(*cp++) ) ; \
                       --cp; }

#ifdef ADJUST_NUMERIC_ONLY_TZ_OFFSETS_WRT_DST
# ifdef TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  ifdef HAVE_SYS_TIME_H
#   include <sys/time.h>
#  else
#   include <time.h>
#  endif
# endif

static void
zonehack (struct tws *tw)
{
    register struct tm *tm;

    if (dmktime (tw) == (time_t) -1)
	return;

    tm = localtime (&tw->tw_clock);
    if (tm->tm_isdst) {
	tw->tw_flags |= TW_DST;
	tw->tw_zone -= 60;
    }
}
#endif	/* ADJUST_NUMERIC_ONLY_TZ_OFFSETS_WRT_DST */
%}

sun	([Ss]un(day)?)
mon	([Mm]on(day)?)
tue	([Tt]ue(sday)?)
wed	([Ww]ed(nesday)?)
thu	([Tt]hu(rsday)?)
fri	([Ff]ri(day)?)
sat	([Ss]at(urday)?)

DAY	({sun}|{mon}|{tue}|{wed}|{thu}|{fri}|{sat})

jan	([Jj]an(uary)?)
feb	([Ff]eb(ruary)?)
mar	([Mm]ar(ch)?)
apr	([Aa]pr(il)?)
may	([Mm]ay)
jun	([Jj]un(e)?)
jul	([Jj]ul(y)?)
aug	([Aa]ug(ust)?)
sep	([Ss]ep(tember)?)
oct	([Oo]ct(ober)?)
nov	([Nn]ov(ember)?)
dec	([Dd]ec(ember)?)

MONTH	({jan}|{feb}|{mar}|{apr}|{may}|{jun}|{jul}|{aug}|{sep}|{oct}|{nov}|{dec})

TIME    ({D}:{d}{d}(:{d}{d})?)

     /* The year can either be 2 digits, or 4. However, after
	Y2K, we found that some MUA were reporting the year 100, hence
	the middle term here. yyterminate() resolves the actual
        issues with 2-digit years.
     */

YEAR    (({d}{d})|(1{d}{d})|({d}{4}))

w	([ \t]*)
W	([ \t]+)
D	([0-9]?[0-9])
d	[0-9]

%%
%{
  /* This section begins the definition of dparsetime().
     Put here any local variable definitions and initializations */
  
  YY_BUFFER_STATE lexhandle;

  register char *cp;
  static struct tws tw; 

  memset(&tw,0,sizeof(struct tws));

  lexhandle = yy_scan_string(lexstr);
%}

{DAY}","?{W}{MONTH}{W}{D}{W}{TIME}{W}{YEAR}   {
                                     INIT();
				     SETWDAY();
				     SKIPTOA();
				     SETMON();
				     SKIPTOD();
				     SETDAY();
				     SKIPTOD();
				     SETTIME();
				     SKIPTOD();
				     SETYEAR();
                                     }

{DAY}","?{W}{D}{W}{MONTH}{W}{YEAR}{W}{TIME}   {
                                     INIT();
				     SETWDAY();
				     SKIPTOD();
				     SETDAY();
				     SKIPTOA();
				     SETMON();
				     SKIPTOD();
				     SETYEAR();
				     SKIPTOD();
				     SETTIME();
                                     }
{D}{W}{MONTH}{W}{YEAR}{W}{TIME}               {
                                     INIT();
				     SETDAY();
				     SKIPTOA();
				     SETMON();
				     SKIPTOD();
				     SETYEAR();
				     SKIPTOD();
				     SETTIME();
                                     }
{DAY}","?{W}{MONTH}{W}{D}","?{W}{YEAR}","?{W}{TIME} {
                                     INIT();
				     SETWDAY();
				     SKIPTOA();
				     SETMON();
				     SKIPTOD();
				     SETDAY();
				     SKIPTOD();
				     SETYEAR();
				     SKIPTOD();
				     SETTIME();
                                     }
{DAY}","?{W}{MONTH}{W}{D}","?{W}{YEAR}        {
                                     INIT();
				     SETWDAY();
				     SKIPTOA();
				     SETMON();
				     SKIPTOD();
				     SETDAY();
				     SKIPTOD();
				     SETYEAR();
                                     }
{MONTH}{W}{D}","?{W}{YEAR}","?{W}{DAY}        {
                                     INIT();
				     SETMON();
				     SKIPTOD();
				     SETDAY();
				     SKIPTOD();
				     SETYEAR();
				     SKIPTOA();
				     SETWDAY();
                                     }
{MONTH}{W}{D}","?{W}{YEAR}                    {
                                     INIT();
				     SETMON();
				     SKIPTOD();
				     SETDAY();
				     SKIPTOD();
				     SETYEAR();
                                     }
{D}("-"|"/"){D}("-"|"/"){YEAR}{W}{TIME}       {
                                     INIT();
				     if(europeandate) {
				       /* DD/MM/YY */
				     SETDAY();
				     SKIPTOD();
				     SETMON_NUM();
				     } else {
				       /* MM/DD/YY */
				     SETMON_NUM();
				     SKIPTOD();
				     SETDAY();
				     }
				     SKIPTOD();
				     SETYEAR();
				     SKIPTOD();
				     SETTIME();
                                     }
{D}("-"|"/"){D}("-"|"/"){YEAR}                {
                                     INIT();
				     if(europeandate) {
				       /* DD/MM/YY */
				     SETDAY();
				     SKIPTOD();
				     SETMON_NUM();
				     } else {
				       /* MM/DD/YY */
				     SETMON_NUM();
				     SKIPTOD();
				     SETDAY();
				     }
				     SKIPTOD();
				     SETYEAR();
                                     }

"[Aa][Mm]"
"[Pp][Mm]"                           tw.tw_hour += 12;

"+"{D}{d}{d}                                  {
                                    INIT();
                                    SKIPTOD();
                                    SETZONE(atoi(cp));
#ifdef	ADJUST_NUMERIC_ONLY_TZ_OFFSETS_WRT_DST
				    zonehack (&tw);
#endif	/* ADJUST_NUMERIC_ONLY_TZ_OFFSETS_WRT_DST */
                                    }
"-"{D}{d}{d}                                  {
                                    INIT();
                                    SKIPTOD();
                                    SETZONE(-atoi(cp));
#ifdef	ADJUST_NUMERIC_ONLY_TZ_OFFSETS_WRT_DST
				    zonehack (&tw);
#endif	/* ADJUST_NUMERIC_ONLY_TZ_OFFSETS_WRT_DST */
                                    }
"-"?("ut"|"UT")			    INIT(); SETZONE(0);
"-"?("gmt"|"GMT")		    INIT(); SETZONE(0);
"-"?("jst"|"JST")		    INIT(); SETZONE(200);
"-"?("jdt"|"JDT")		    INIT(); SETDST(); SETZONE(2);
"-"?("est"|"EST")		    INIT(); SETZONE(-500);
"-"?("edt"|"EDT")		    INIT(); SETDST(); SETZONE(-500);
"-"?("cst"|"CST")		    INIT(); SETZONE(-600);
"-"?("cdt"|"CDT")		    INIT(); SETDST(); SETZONE(-600);
"-"?("mst"|"MST")		    INIT(); SETZONE(-700);
"-"?("mdt"|"MDT")		    INIT(); SETDST(); SETZONE(-700);
"-"?("pst"|"PST")		    INIT(); SETZONE(-800);
"-"?("pdt"|"PDT")		    INIT(); SETDST(); SETZONE(-800);
"-"?("nst"|"NST")		    INIT(); SETZONE(-330);
"-"?("ast"|"AST")		    INIT(); SETZONE(-400);
"-"?("adt"|"ADT")		    INIT(); SETDST(); SETZONE(-400);
"-"?("yst"|"YST")		    INIT(); SETZONE(-900);
"-"?("ydt"|"YDT")		    INIT(); SETDST(); SETZONE(-900);
"-"?("hst"|"HST")		    INIT(); SETZONE(-1000);
"-"?("hdt"|"HDT")		    INIT(); SETDST(); SETZONE(-1000);
"-"?("bst"|"BST")		    INIT(); SETDST(); SETZONE(-100);
[a-iA-I]			    {
                                       INIT();
                                       SETZONE(100*(('a'-1) - tolower(*cp)));
			            }
[k-mK-M]			    {
                                       INIT();
                                       SETZONE(100*('a' - tolower(*cp)));
			            }
[n-yN-Y]		            {
                                       INIT();
                                       SETZONE(100*(tolower(*cp) - 'm'));
                                    }
.|\n
