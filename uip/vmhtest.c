
/*
 * vmhtest.c -- test out vmh protocol
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/vmhsbr.h>

static struct swit switches[] = {
#define	READSW          0
    { "vmhread fd", 7 },	
#define	WRITESW         1
    { "vmhwrite fd", 8 },	
#define VERSIONSW       2
    { "version", 0 },
#define	HELPSW          3
    { "help", 4 },
    { NULL, NULL }
};

#define	NWIN 20
static int numwins = 0;
static int windows[NWIN + 1];


static int selcmds = 0;
#define	selcmd()	(selcmds++ % 2)

static int selwins = 0;
#define	selwin()	(selwins++ % 2 ? 3 : 1)


int
main (int argc, char **argv)
{
    int fd1, fd2;
    char *cp, buffer[BUFSIZ];
    char **argp, **arguments;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* foil search of user profile/context */
    if (context_foil (NULL) == -1)
	done (1);

    arguments = getarguments (invo_name, argc, argv, 0);
    argp = arguments;

    while ((cp = *argp++))
	if (*cp == '-')
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buffer, sizeof(buffer), "%s [switches]", invo_name);
		    print_help (buffer, switches, 0);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case READSW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((fd1 = atoi (cp)) < 1)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;
		case WRITESW: 
		    if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    if ((fd2 = atoi (cp)) < 1)
			adios (NULL, "bad argument %s %s", argp[-2], cp);
		    continue;
	    }
	else
	    adios (NULL, "usage: %s [switches]", invo_name);

    rcinit (fd1, fd2);
    pINI ();
    pLOOP ();

    return done (0);
}


static int  pINI () {
    int     i,
            vrsn;
    char   *bp;
    struct record   rcs,
                   *rc = &rcs;

    initrc (rc);

    switch (peer2rc (rc)) {
	case RC_INI: 
	    bp = rc->rc_data;
	    while (isspace (*bp))
		bp++;
	    if (sscanf (bp, "%d", &vrsn) != 1) {
	bad_init: ;
		fmt2peer (RC_ERR, "bad init \"%s\"", rc->rc_data);
		done (1);
	    }
	    if (vrsn != RC_VRSN) {
		fmt2peer (RC_ERR, "version %d unsupported", vrsn);
		done (1);
	    }

	    while (*bp && !isspace (*bp))
		bp++;
	    while (isspace (*bp))
		bp++;
	    if (sscanf (bp, "%d", &numwins) != 1 || numwins <= 0)
		goto bad_init;
	    if (numwins > NWIN)
		numwins = NWIN;

	    for (i = 1; i <= numwins; i++) {
		while (*bp && !isspace (*bp))
		    bp++;
		while (isspace (*bp))
		    bp++;
		if (sscanf (bp, "%d", &windows[i]) != 1 || windows[i] <= 0)
		    goto bad_init;
	    }
	    rc2peer (RC_ACK, 0, NULL);
	    return OK;

	case RC_XXX: 
	    adios (NULL, "%s", rc->rc_data);

	default: 
	    fmt2peer (RC_ERR, "pINI protocol screw-up");
	    done (1);		/* NOTREACHED */
    }
}


static int  pLOOP () {
    struct record   rcs,
                   *rc = &rcs;

    initrc (rc);

    for (;;)
	switch (peer2rc (rc)) {
	    case RC_QRY: 
		pQRY (rc->rc_data);
		break;

	    case RC_CMD: 
		pCMD (rc->rc_data);
		break;

	    case RC_FIN: 
		done (0);

	    case RC_XXX: 
		adios (NULL, "%s", rc->rc_data);

	    default: 
		fmt2peer (RC_ERR, "pLOOP protocol screw-up");
		done (1);
	}
}


static int  pQRY (str)
char   *str;
{
    rc2peer (RC_EOF, 0, NULL);
    return OK;
}


static int  pCMD (str)
char   *str;
{
    if ((selcmd () ? pTTY (str) : pWIN (str)) == NOTOK)
	return NOTOK;
    rc2peer (RC_EOF, 0, NULL);
    return OK;
}


static int  pTTY (str)
char   *str;
{
    struct record   rcs,
                   *rc = &rcs;

    initrc (rc);

    switch (rc2rc (RC_TTY, 0, NULL, rc)) {
	case RC_ACK: 
	    break;

	case RC_ERR: 
	    return NOTOK;

	case RC_XXX: 
	    adios (NULL, "%s", rc->rc_data);

	default: 
	    fmt2peer (RC_ERR, "pTTY protocol screw-up");
	    done (1);
    }

    system (str);

    switch (rc2rc (RC_EOF, 0, NULL, rc)) {
	case RC_ACK: 
	    return OK;

	case RC_XXX: 
	    adios (NULL, "%s", rc->rc_data);/* NOTREACHED */

	default: 
	    fmt2peer (RC_ERR, "pTTY protocol screw-up");
	    done (1);		/* NOTREACHED */
    }
}


static int  pWIN (str)
char   *str;
{
    int     i,
            pid,
            pd[2];
    char    buffer[BUFSIZ];
    struct record   rcs,
                   *rc = &rcs;

    initrc (rc);

    snprintf (buffer, sizeof(buffer), "%d", selwin ());
    switch (str2rc (RC_WIN, buffer, rc)) {
	case RC_ACK: 
	    break;

	case RC_ERR: 
	    return NOTOK;

	case RC_XXX: 
	    adios (NULL, "%s", rc->rc_data);

	default: 
	    fmt2peer (RC_ERR, "pWIN protocol screw-up");
	    done (1);
    }

    if (pipe (pd) == NOTOK) {
	fmt2peer (RC_ERR, "no pipes");
	return NOTOK;
    }

    switch (pid = vfork ()) {
	case NOTOK: 
	    fmt2peer (RC_ERR, "no forks");
	    return NOTOK;

	case OK: 
	    close (0);
	    open ("/dev/null", O_RDONLY);
	    dup2 (pd[1], 1);
	    dup2 (pd[1], 2);
	    close (pd[0]);
	    close (pd[1]);
	    execlp ("/bin/sh", "sh", "-c", str, NULL);
	    write (2, "no shell\n", strlen ("no shell\n"));
	    _exit (1);

	default: 
	    close (pd[1]);
	    while ((i = read (pd[0], buffer, sizeof buffer)) > 0)
		switch (rc2rc (RC_DATA, i, buffer, rc)) {
		    case RC_ACK: 
			break;

		    case RC_ERR: 
			close (pd[0]);
			pidwait (pid, OK);
			return NOTOK;

		    case RC_XXX: 
			adios (NULL, "%s", rc->rc_data);

		    default: 
			fmt2peer (RC_ERR, "pWIN protocol screw-up");
			done (1);
		}
	    if (i == OK)
		switch (rc2rc (RC_EOF, 0, NULL, rc)) {
		    case RC_ACK: 
			break;

		    case RC_XXX: 
			adios (NULL, "%s", rc->rc_data);

		    default: 
			fmt2peer (RC_ERR, "pWIN protocol screw-up");
			done (1);
		}
	    if (i == NOTOK)
		fmt2peer (RC_ERR, "read from pipe lost");

	    close (pd[0]);
	    pidwait (pid, OK);
	    return (i != NOTOK ? OK : NOTOK);
    }
}
