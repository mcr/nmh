
/*
 * dp.c -- parse dates 822-style
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/fmt_scan.h>
#include <h/tws.h>

#define	NDATES 100

#define	WIDTH 78
#define	WBUFSIZ BUFSIZ

#define	FORMAT "%<(nodate{text})error: %{text}%|%(putstr(pretty{text}))%>"

static struct swit switches[] = {
#define	FORMSW                0
    { "form formatfile", 0 },
#define	FMTSW                 1
    { "format string", 5 },
#define	WIDTHSW               2
    { "width columns", 0 },
#define VERSIONSW             3
    { "version", 0 },
#define	HELPSW                4
    { "help", 0 },
    { NULL, 0 }
};

static struct format *fmt;

static int dat[5];

/*
 * prototypes
 */
int sc_width (void);  /* from termsbr.c */

/*
 * static prototypes
 */
static int process (char *, int);


int
main (int argc, char **argv)
{
    int datep = 0, width = 0, status = 0;
    char *cp, *form = NULL, *format = NULL, *nfs;
    char buf[BUFSIZ], **argp, **arguments;
    char *dates[NDATES];

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] dates ...",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    format = NULL;
		    continue;
		case FMTSW: 
		    if (!(format = *argp++) || *format == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    form = NULL;
		    continue;

		case WIDTHSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    width = atoi (cp);
		    continue;
	    }
	}
	if (datep > NDATES)
	    adios (NULL, "more than %d dates", NDATES);
	else
	    dates[datep++] = cp;
    }
    dates[datep] = NULL;

    if (datep == 0)
	adios (NULL, "usage: %s [switches] dates ...", invo_name);

    /* get new format string */
    nfs = new_fs (form, format, FORMAT);

    if (width == 0) {
	if ((width = sc_width ()) < WIDTH / 2)
	    width = WIDTH / 2;
	width -= 2;
    }
    if (width > WBUFSIZ)
	width = WBUFSIZ;
    fmt_compile (nfs, &fmt);

    dat[0] = 0;
    dat[1] = 0;
    dat[2] = 0;
    dat[3] = width;
    dat[4] = 0;

    for (datep = 0; dates[datep]; datep++)
	status += process (dates[datep], width);

    context_save ();	/* save the context file */
    return done (status);
}


static int
process (char *date, int length)
{
    int status = 0;
    char buffer[WBUFSIZ + 1];
    register struct comp *cptr;

    FINDCOMP (cptr, "text");
    if (cptr)
	cptr->c_text = date;
    fmt_scan (fmt, buffer, length, dat);
    fputs (buffer, stdout);

    return status;
}
