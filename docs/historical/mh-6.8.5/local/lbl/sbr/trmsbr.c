/* trmsbr.c - minor termcap support (load with -ltermlib) */

#include "../h/mh.h"
#include <stdio.h>
#ifndef	SYS5
#include <sgtty.h>
#else	SYS5
#include <sys/types.h>
#include <termio.h>
#include <sys/ioctl.h>
#endif	SYS5
#include <ctype.h>


#if	BUFSIZ<2048
#define	TXTSIZ	2048
#else
#define	TXTSIZ	BUFSIZ
#endif

char	PC;
short	ospeed;

int     tgetent (), tgetnum ();
char   *tgetstr ();

/*  */

static int  TCinited = 0;
static int  TCavailable = 0;
static int  initLI = 0;
static int  LI = 24;
static int  initCO = 0;
static int  CO = 80;
static char *CL = NULL;
static char *SE = NULL;
static char *SO = NULL;

static char termcap[TXTSIZ];
static char area[TXTSIZ];

/*  */

static void
read_termcap()
{
    register char  *bp,
                   *term;
    register int i;
    char   *cp;
#ifndef	SYS5
    struct sgttyb   sg;
#else	SYS5
    struct termio   sg;
#endif	SYS5

    if (TCinited)
	return;

    ++TCinited;

    if (!isatty(fileno(stdout)))
	return;

    if ((term = getenv ("TERM")) == NULL || tgetent (termcap, term) <= OK)
	return;

    ++TCavailable;

#ifndef	SYS5
    ospeed = ioctl(fileno (stdout), TIOCGETP, (char *) &sg) != NOTOK
		? sg.sg_ospeed : 0;
#else	SYS5
    ospeed = ioctl(fileno (stdout), TCGETA, &sg) != NOTOK
		? sg.c_cflag & CBAUD : 0;
#endif	SYS5

    if (!initCO && (i = tgetnum ("co")) > 0)
	CO = i;
    if (!initLI && (i = tgetnum ("li")) > 0)
	LI = i;

    cp = area;
    CL = tgetstr ("cl", &cp);
    if (bp = tgetstr ("pc", &cp))
	PC = *bp;
    if (tgetnum ("sg") <= 0) {
	SE = tgetstr ("se", &cp);
	SO = tgetstr ("so", &cp);
    }
}

/*  */

int
sc_width()
{
#ifdef	TIOCGWINSZ
    struct winsize win;

    if (ioctl(fileno (stderr), TIOCGWINSZ, &win) != NOTOK && win.ws_col > 0) {
	CO = win.ws_col;
	initCO++;
    } else
#endif	TIOCGWINSZ
	read_termcap();

    return CO;
}


int
sc_length()
{
#ifdef	TIOCGWINSZ
    struct winsize win;

    if (ioctl(fileno (stderr), TIOCGWINSZ, &win) != NOTOK && win.ws_row > 0) {
	LI = win.ws_row;
	initLI++;
    } else
#endif	TIOCGWINSZ
	read_termcap();

    return LI;
}

/*  */

static int
outc(c)
register char    c;
{
    (void) putchar (c);
}


void
clear_screen()
{
    read_termcap();

    if (CL && ospeed)
	tputs (CL, LI, outc);
    else {
	printf ("\f");
	if (ospeed)
	    printf ("\200");
    }

    (void) fflush (stdout);
}

/*  */

/* VARARGS1 */

SOprintf(fmt, a, b, c, d, e, f)
char   *fmt,
       *a,
       *b,
       *c,
       *d,
       *e,
       *f;
{
    read_termcap();
    if (SO == NULL || SE == NULL)
	return NOTOK;

    tputs (SO, 1, outc);
    printf (fmt, a, b, c, d, e, f);
    tputs (SE, 1, outc);

    return OK;
}

#define MAXTCENT 32
static int nTCent;
static char *TCid[MAXTCENT];
static char *TCent[MAXTCENT];

char *
get_termcap(id)
	char *id;
{
	register int i;
	char *cp;
	static char tcbuf[1024];
	static char *tcptr = tcbuf;

	for (i = 0; i < nTCent; ++i)
		if (strcmp(TCid[i], id) == 0)
			return (TCent[i]);

	if (nTCent >= MAXTCENT)
		return ("");

	TCid[nTCent] = getcpy(id);

	if (TCinited == 0)
		read_termcap();

	if (!TCavailable)
		return ("");

	cp = tgetstr(id, &tcptr);
	if (cp == 0) {
		/* check for some alternates for the entry */
		if (strcmp(id, "mb") == 0 ||
		    strcmp(id, "md") == 0 ||
		    strcmp(id, "mh") == 0 ||
		    strcmp(id, "mr") == 0 ||
		    strcmp(id, "us") == 0)
			id = "so";
		else if (strcmp(id, "me") == 0 ||
		         strcmp(id, "ue") == 0)
			id = "se";
		cp = tgetstr(id, &tcptr);
		if (cp == 0)
			cp = "";
	}

	/* skip leading delay spec */
	while (isdigit(*cp))
		++cp;
	if (*cp == '.')
		if (isdigit(*++cp))
			++cp;
	if (*cp == '*')
		++cp;

	TCent[nTCent] = cp;
	++nTCent;
	return (cp);
}
