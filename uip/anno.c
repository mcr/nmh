
/*
 * anno.c -- annotate messages
 *
 * $Id$
 */

#include <h/mh.h>

/*
 * We allocate space for messages (msgs array)
 * this number of elements at a time.
 */
#define MAXMSGS  256


static struct swit switches[] = {
#define	COMPSW	0
    { "component field", 0 },
#define	INPLSW	1
    { "inplace", 0 },
#define	NINPLSW	2
    { "noinplace", 0 },
#define	DATESW	3
    { "date", 0 },
#define	NDATESW	4
    { "nodate", 0 },
#define	TEXTSW	5
    { "text body", 0 },
#define VERSIONSW 6
    { "version", 0 },
#define	HELPSW	7
    { "help", 4 },
    { NULL, 0 }
};

/*
 * static prototypes
 */
static void make_comp (char **);


int
main (int argc, char **argv)
{
    int inplace = 1, datesw = 1;
    int nummsgs, maxmsgs, msgnum;
    char *cp, *maildir, *comp = NULL;
    char *text = NULL, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments, **msgs;
    struct msgs *mp;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Allocate the initial space to record message
     * names and ranges.
     */
    nummsgs = 0;
    maxmsgs = MAXMSGS;
    if (!(msgs = (char **) malloc ((size_t) (maxmsgs * sizeof(*msgs)))))
	adios (NULL, "unable to allocate storage");

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case COMPSW: 
		    if (comp)
			adios (NULL, "only one component at a time!");
		    if (!(comp = *argp++) || *comp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case DATESW: 
		    datesw++;
		    continue;
		case NDATESW: 
		    datesw = 0;
		    continue;

		case INPLSW: 
		    inplace++;
		    continue;
		case NINPLSW: 
		    inplace = 0;
		    continue;

		case TEXTSW: 
		    if (text)
			adios (NULL, "only one body at a time!");
		    if (!(text = *argp++) || *text == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    /*
	     * Check if we need to allocate more space
	     * for message name/ranges.
	     */
	    if (nummsgs >= maxmsgs) {
		maxmsgs += MAXMSGS;
		if (!(msgs = (char **) realloc (msgs,
			(size_t) (maxmsgs * sizeof(*msgs)))))
		    adios (NULL, "unable to reallocate msgs storage");
	    }
	    msgs[nummsgs++] = cp;
	}
    }

#ifdef UCI
    if (strcmp(invo_name, "fanno") == 0)	/* ugh! */
	datesw = 0;
#endif	/* UCI */

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!nummsgs)
	msgs[nummsgs++] = "cur";
    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	adios (NULL, "no messages in %s", folder);

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < nummsgs; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    done (1);

    make_comp (&comp);

    /* annotate all the SELECTED messages */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected(mp, msgnum))
	    annotate (m_name (msgnum), comp, text, inplace, datesw);

    context_replace (pfolder, folder);	/* update current folder  */
    seq_setcur (mp, mp->lowsel);	/* update current message */
    seq_save (mp);	/* synchronize message sequences */
    folder_free (mp);	/* free folder/message structure */
    context_save ();	/* save the context file         */
    done (0);
}


static void
make_comp (char **ap)
{
    register char *cp;
    char buffer[BUFSIZ];

    if (*ap == NULL) {
	printf ("Enter component name: ");
	fflush (stdout);

	if (fgets (buffer, sizeof buffer, stdin) == NULL)
	    done (1);
	*ap = trimcpy (buffer);
    }

    if ((cp = *ap + strlen (*ap) - 1) > *ap && *cp == ':')
	*cp = 0;
    if (strlen (*ap) == 0)
	adios (NULL, "null component name");
    if (**ap == '-')
	adios (NULL, "invalid component name %s", *ap);
    if (strlen (*ap) >= NAMESZ)
	adios (NULL, "too large component name %s", *ap);

    for (cp = *ap; *cp; cp++)
	if (!isalnum (*cp) && *cp != '-')
	    adios (NULL, "invalid component name %s", *ap);
}

