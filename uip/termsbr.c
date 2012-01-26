
/*
 * termsbr.c -- termcap support
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#include <termios.h>

/* It might be better to tie this to the termcap_curses_order in
   configure.ac.  It would be fine to check for ncurses/termcap.h
   first on Linux, it's a symlink to termcap.h.  */
#ifdef HAVE_TERMCAP_H
# include <termcap.h>
#elif defined (HAVE_NCURSES_TERMCAP_H)
# include <ncurses/termcap.h>
#endif

/* <sys/ioctl.h> is need anyway for ioctl()
#ifdef GWINSZ_IN_SYS_IOCTL
*/
# include <sys/ioctl.h>
/*
#endif
*/

#ifdef WINSIZE_IN_PTEM
# include <sys/stream.h>
# include <sys/ptem.h>
#endif

#if BUFSIZ<2048
# define TXTSIZ	2048
#else
# define TXTSIZ BUFSIZ
#endif

static long speedcode;

static int initLI = 0;
static int initCO = 0;

static int HC = 0;       /* are we on a hardcopy terminal?        */
static int LI = 40;      /* number of lines                       */
static int CO = 80;      /* number of colums                      */
static char *CL = NULL;  /* termcap string to clear screen        */
static char *SE = NULL;  /* termcap string to end standout mode   */
static char *SO = NULL;  /* termcap string to begin standout mode */

static char termcap[TXTSIZ];


static void
read_termcap(void)
{
    char *bp, *cp;
    char *term;

#ifndef TGETENT_ACCEPTS_NULL
    char termbuf[TXTSIZ];
#endif

    struct termios tio;
    static int inited = 0;

    if (inited++)
	return;

    if (!(term = getenv ("TERM")))
	return;

/*
 * If possible, we let tgetent allocate its own termcap buffer
 */
#ifdef TGETENT_ACCEPTS_NULL
    if (tgetent (NULL, term) != TGETENT_SUCCESS)
	return;
#else
    if (tgetent (termbuf, term) != TGETENT_SUCCESS)
	return;
#endif

    speedcode = cfgetospeed(&tio);

    HC = tgetflag ("hc");

    if (!initCO && (CO = tgetnum ("co")) <= 0)
	CO = 80;
    if (!initLI && (LI = tgetnum ("li")) <= 0)
	LI = 24;

    cp = termcap;
    CL = tgetstr ("cl", &cp);
    if (tgetnum ("sg") <= 0) {
	SE = tgetstr ("se", &cp);
	SO = tgetstr ("so", &cp);
    }
}


int
sc_width (void)
{
#ifdef TIOCGWINSZ
    struct winsize win;
    int width;

    if (ioctl (fileno (stderr), TIOCGWINSZ, &win) != NOTOK
	    && (width = win.ws_col) > 0) {
	CO = width;
	initCO++;
    } else
#endif /* TIOCGWINSZ */
	read_termcap();

    return CO;
}


int
sc_length (void)
{
#ifdef TIOCGWINSZ
    struct winsize win;

    if (ioctl (fileno (stderr), TIOCGWINSZ, &win) != NOTOK
	    && (LI = win.ws_row) > 0)
	initLI++;
    else
#endif /* TIOCGWINSZ */
	read_termcap();

    return LI;
}


static int
outc (int c)
{
    return putchar(c);
}


void
clear_screen (void)
{
    read_termcap ();

    if (CL && speedcode)
	tputs (CL, LI, outc);
    else {
	printf ("\f");
	if (speedcode)
	    printf ("\200");
    }

    fflush (stdout);
}


/*
 * print in standout mode
 */
int
SOprintf (char *fmt, ...)
{
    va_list ap;

    read_termcap ();
    if (!(SO && SE))
	return NOTOK;

    tputs (SO, 1, outc);

    va_start(ap, fmt);
    vprintf (fmt, ap);
    va_end(ap);

    tputs (SE, 1, outc);

    return OK;
}

/*
 * Is this a hardcopy terminal?
 */

int
sc_hardcopy(void)
{
    read_termcap();
    return HC;
}

