
/*
 * popsbr.c -- POP client subroutines
 *
 * $Id$
 */

#include <h/mh.h>

#if defined(NNTP) && !defined(PSHSBR)
# undef NNTP
#endif

#ifdef NNTP			/* building pshsbr.o from popsbr.c */
# include <h/nntp.h>
#endif /* NNTP */

#if !defined(NNTP) && defined(APOP)
# include <h/md5.h>
#endif

#include <h/popsbr.h>
#include <h/signals.h>
#include <signal.h>

#define	TRM	"."
#define	TRMLEN	(sizeof TRM - 1)

extern int errno;

static int poprint = 0;
static int pophack = 0;

char response[BUFSIZ];

static FILE *input;
static FILE *output;

#define	targ_t char *

#if !defined(NNTP) && defined(MPOP)
# define command pop_command
# define multiline pop_multiline
#endif

#ifdef NNTP
# ifdef BPOP	/* stupid */
static int xtnd_last = -1;
static int xtnd_first = 0;
static char xtnd_name[512];	/* INCREDIBLE HACK!! */
# endif
#endif /* NNTP */

/*
 * static prototypes
 */
#if !defined(NNTP) && defined(APOP)
static char *pop_auth (char *, char *);
#endif

#if defined(NNTP) || !defined(MPOP)
/* otherwise they are not static functions */
static int command(const char *, ...);
static int multiline(void);
#endif

static int traverse (int (*)(), const char *, ...);
static int vcommand(const char *, va_list);
static int getline (char *, int, FILE *);
static int putline (char *, FILE *);


#if !defined(NNTP) && defined(APOP)
static char *
pop_auth (char *user, char *pass)
{
    int len, buflen;
    char *cp, *lp;
    unsigned char *dp, *ep, digest[16];
    MD5_CTX mdContext;
    static char buffer[BUFSIZ];

    if ((cp = strchr (response, '<')) == NULL
	    || (lp = strchr (cp, '>')) == NULL) {
	snprintf (buffer, sizeof(buffer), "APOP not available: %s", response);
	strncpy (response, buffer, sizeof(response));
	return NULL;
    }

    *++lp = NULL;
    snprintf (buffer, sizeof(buffer), "%s%s", cp, pass);

    MD5Init (&mdContext);
    MD5Update (&mdContext, (unsigned char *) buffer,
	       (unsigned int) strlen (buffer));
    MD5Final (digest, &mdContext);

    cp = buffer;
    buflen = sizeof(buffer);

    snprintf (cp, buflen, "%s ", user);
    len = strlen (cp);
    cp += len;
    buflen -= len;

    for (ep = (dp = digest) + sizeof(digest) / sizeof(digest[0]); dp < ep; ) {
	snprintf (cp, buflen, "%02x", *dp++ & 0xff);
	cp += 2;
	buflen -= 2;
    }
    *cp = NULL;

    return buffer;
}
#endif	/* !NNTP && APOP */


int
pop_init (char *host, char *user, char *pass, int snoop, int rpop)
{
    int fd1, fd2;
    char buffer[BUFSIZ];

#ifdef APOP
    int apop;

    if ((apop = rpop) < 0)
	rpop = 0;
#endif

#ifndef NNTP
# ifndef KPOP
    if ((fd1 = client (host, "tcp", POPSERVICE, rpop, response, sizeof(response))) == NOTOK)
# else	/* KPOP */
    snprintf (buffer, sizeof(buffer), "%s/%s", KPOP_PRINCIPAL, POPSERVICE);
    if ((fd1 = client (host, "tcp", buffer, rpop, response, sizeof(response))) == NOTOK)
# endif
#else	/* NNTP */
    if ((fd1 = client (host, "tcp", "nntp", rpop, response, sizeof(response))) == NOTOK)
#endif
	return NOTOK;

    if ((fd2 = dup (fd1)) == NOTOK) {
	char *s;

	if ((s = strerror(errno)))
	    snprintf (response, sizeof(response),
		"unable to dup connection descriptor: %s", s);
	else
	    snprintf (response, sizeof(response),
		"unable to dup connection descriptor: unknown error");
	close (fd1);
	return NOTOK;
    }
#ifndef NNTP
    if (pop_set (fd1, fd2, snoop) == NOTOK)
#else	/* NNTP */
    if (pop_set (fd1, fd2, snoop, (char *)0) == NOTOK)
#endif	/* NNTP */
	return NOTOK;

    SIGNAL (SIGPIPE, SIG_IGN);

    switch (getline (response, sizeof response, input)) {
	case OK: 
	    if (poprint)
		fprintf (stderr, "<--- %s\n", response);
#ifndef	NNTP
	    if (*response == '+') {
# ifndef KPOP
#  ifdef APOP
		if (apop < 0) {
		    char *cp = pop_auth (user, pass);

		    if (cp && command ("APOP %s", cp) != NOTOK)
			return OK;
		}
		else
#  endif /* APOP */
		if (command ("USER %s", user) != NOTOK
		    && command ("%s %s", rpop ? "RPOP" : (pophack++, "PASS"),
					pass) != NOTOK)
		return OK;
# else /* KPOP */
		if (command ("USER %s", user) != NOTOK
		    && command ("PASS %s", pass) != NOTOK)
		return OK;
# endif
	    }
#else /* NNTP */
	    if (*response < CHAR_ERR) {
		command ("MODE READER");
		return OK;
	    }
#endif
	    strncpy (buffer, response, sizeof(buffer));
	    command ("QUIT");
	    strncpy (response, buffer, sizeof(response));
				/* and fall */

	case NOTOK: 
	case DONE: 
	    if (poprint)	    
		fprintf (stderr, "%s\n", response);
	    fclose (input);
	    fclose (output);
	    return NOTOK;
    }

    return NOTOK;	/* NOTREACHED */
}

#ifdef NNTP
int
pop_set (int in, int out, int snoop, char *myname)
#else
int
pop_set (int in, int out, int snoop)
#endif
{

#ifdef NNTP
    if (myname && *myname) {
	/* interface from bbc to msh */
	strncpy (xtnd_name, myname, sizeof(xtnd_name));
    }
#endif	/* NNTP */

    if ((input = fdopen (in, "r")) == NULL
	    || (output = fdopen (out, "w")) == NULL) {
	strncpy (response, "fdopen failed on connection descriptor", sizeof(response));
	if (input)
	    fclose (input);
	else
	    close (in);
	close (out);
	return NOTOK;
    }

    poprint = snoop;

    return OK;
}


int
pop_fd (char *in, int inlen, char *out, int outlen)
{
    snprintf (in, inlen, "%d", fileno (input));
    snprintf (out, outlen, "%d", fileno (output));
    return OK;
}


/*
 * Find out number of messages available
 * and their total size.
 */

int
pop_stat (int *nmsgs, int *nbytes)
{
#ifdef NNTP
    char **ap;
#endif /* NNTP */

#ifndef	NNTP
    if (command ("STAT") == NOTOK)
	return NOTOK;

    *nmsgs = *nbytes = 0;
    sscanf (response, "+OK %d %d", nmsgs, nbytes);

#else /* NNTP */
    if (xtnd_last < 0) { 	/* in msh, xtnd_name is set from myname */
	if (command("GROUP %s", xtnd_name) == NOTOK)
	    return NOTOK;

	ap = brkstring (response, " ", "\n"); /* "211 nart first last ggg" */
	xtnd_first = atoi (ap[2]);
	xtnd_last  = atoi (ap[3]);
    }

    /* nmsgs is not the real nart, but an incredible simuation */
    if (xtnd_last > 0)
	*nmsgs = xtnd_last - xtnd_first + 1;	/* because of holes... */
    else
	*nmsgs = 0;
    *nbytes = xtnd_first;	/* for subtracting offset in msh() */
#endif /* NNTP */

    return OK;
}

#ifdef NNTP
int
pop_exists (int (*action)())
{
#ifdef XMSGS		/* hacked into NNTP 1.5 */
    if (traverse (action, "XMSGS %d-%d", (targ_t) xtnd_first, (targ_t) xtnd_last) == OK)
	return OK;
#endif
    /* provided by INN 1.4 */
    if (traverse (action, "LISTGROUP") == OK)
	return OK;
    return traverse (action, "XHDR NONAME %d-%d", (targ_t) xtnd_first, (targ_t) xtnd_last);
}
#endif	/* NNTP */


#ifdef BPOP
int
pop_list (int msgno, int *nmsgs, int *msgs, int *bytes, int *ids)
#else
int
pop_list (int msgno, int *nmsgs, int *msgs, int *bytes)
#endif
{
    int i;
#ifndef	BPOP
    int *ids = NULL;
#endif

    if (msgno) {
#ifndef NNTP
	if (command ("LIST %d", msgno) == NOTOK)
	    return NOTOK;
	*msgs = *bytes = 0;
	if (ids) {
	    *ids = 0;
	    sscanf (response, "+OK %d %d %d", msgs, bytes, ids);
	}
	else
	    sscanf (response, "+OK %d %d", msgs, bytes);
#else /* NNTP */
	*msgs = *bytes = 0;
	if (command ("STAT %d", msgno) == NOTOK) 
	    return NOTOK;
	if (ids) {
	    *ids = msgno;
	}
#endif /* NNTP */
	return OK;
    }

#ifndef NNTP
    if (command ("LIST") == NOTOK)
	return NOTOK;

    for (i = 0; i < *nmsgs; i++)
	switch (multiline ()) {
	    case NOTOK: 
		return NOTOK;
	    case DONE: 
		*nmsgs = ++i;
		return OK;
	    case OK: 
		*msgs = *bytes = 0;
		if (ids) {
		    *ids = 0;
		    sscanf (response, "%d %d %d",
			    msgs++, bytes++, ids++);
		}
		else
		    sscanf (response, "%d %d", msgs++, bytes++);
		break;
	}
    for (;;)
	switch (multiline ()) {
	    case NOTOK: 
		return NOTOK;
	    case DONE: 
		return OK;
	    case OK: 
		break;
	}
#else /* NNTP */
    return NOTOK;
#endif /* NNTP */
}


int
pop_retr (int msgno, int (*action)())
{
#ifndef NNTP
    return traverse (action, "RETR %d", (targ_t) msgno);
#else /* NNTP */
    return traverse (action, "ARTICLE %d", (targ_t) msgno);
#endif /* NNTP */
}


static int
traverse (int (*action)(), const char *fmt, ...)
{
    int result;
    va_list ap;
    char buffer[sizeof(response)];

    va_start(ap, fmt);
    result = vcommand (fmt, ap);
    va_end(ap);

    if (result == NOTOK)
	return NOTOK;
    strncpy (buffer, response, sizeof(buffer));

    for (;;)
	switch (multiline ()) {
	    case NOTOK: 
		return NOTOK;

	    case DONE: 
		strncpy (response, buffer, sizeof(response));
		return OK;

	    case OK: 
		(*action) (response);
		break;
	}
}


int
pop_dele (int msgno)
{
    return command ("DELE %d", msgno);
}


int
pop_noop (void)
{
    return command ("NOOP");
}


#if defined(MPOP) && !defined(NNTP)
int
pop_last (void)
{
    return command ("LAST");
}
#endif


int
pop_rset (void)
{
    return command ("RSET");
}


int
pop_top (int msgno, int lines, int (*action)())
{
#ifndef NNTP
    return traverse (action, "TOP %d %d", (targ_t) msgno, (targ_t) lines);
#else	/* NNTP */
    return traverse (action, "HEAD %d", (targ_t) msgno);
#endif /* NNTP */
}


#ifdef BPOP
int
pop_xtnd (int (*action)(), char *fmt, ...)
{
    int result;
    va_list ap;
    char buffer[BUFSIZ];

#ifdef NNTP
    char **ap;
#endif

    va_start(ap, fmt);
#ifndef NNTP
    /* needs to be fixed... va_end needs to be added */
    snprintf (buffer, sizeof(buffer), "XTND %s", fmt);
    result = traverse (action, buffer, a, b, c, d);
    va_end(ap);
    return result;
#else /* NNTP */
    snprintf (buffer, sizeof(buffer), fmt, a, b, c, d);
    ap = brkstring (buffer, " ", "\n");	/* a hack, i know... */

    if (!strcasecmp(ap[0], "x-bboards")) {	/* XTND "X-BBOARDS group */
	/* most of these parameters are meaningless under NNTP. 
	 * bbc.c was modified to set AKA and LEADERS as appropriate,
	 * the rest are left blank.
	 */
	return OK;
    }
    if (!strcasecmp (ap[0], "archive") && ap[1]) {
	snprintf (xtnd_name, sizeof(xtnd_name), "%s", ap[1]);	/* save the name */
	xtnd_last = 0;
	xtnd_first = 1;		/* setup to fail in pop_stat */
	return OK;
    }
    if (!strcasecmp (ap[0], "bboards")) {

	if (ap[1]) {			/* XTND "BBOARDS group" */
	    snprintf (xtnd_name, sizeof(xtnd_name), "%s", ap[1]);	/* save the name */
	    if (command("GROUP %s", xtnd_name) == NOTOK)
		return NOTOK;

	    /* action must ignore extra args */
	    strncpy (buffer, response, sizeof(buffer));
	    ap = brkstring (response, " ", "\n");/* "211 nart first last g" */
	    xtnd_first = atoi (ap[2]);
	    xtnd_last  = atoi (ap[3]);

	    (*action) (buffer);		
	    return OK;

	} else {		/* XTND "BBOARDS" */
	    return traverse (action, "LIST", a, b, c, d);
	}
    }
    return NOTOK;	/* unknown XTND command */
#endif /* NNTP */
}
#endif BPOP


int
pop_quit (void)
{
    int i;

    i = command ("QUIT");
    pop_done ();

    return i;
}


int
pop_done (void)
{
    fclose (input);
    fclose (output);

    return OK;
}


#if !defined(MPOP) || defined(NNTP)
static
#endif
int
command(const char *fmt, ...)
{
    va_list ap;
    int result;

    va_start(ap, fmt);
    result = vcommand(fmt, ap);
    va_end(ap);

    return result;
}


static int
vcommand (const char *fmt, va_list ap)
{
    char *cp, buffer[BUFSIZ];

    vsnprintf (buffer, sizeof(buffer), fmt, ap);
    if (poprint)
	if (pophack) {
	    if ((cp = strchr (buffer, ' ')))
		*cp = 0;
	    fprintf (stderr, "---> %s ********\n", buffer);
	    if (cp)
		*cp = ' ';
	    pophack = 0;
	}
	else
	    fprintf (stderr, "---> %s\n", buffer);

    if (putline (buffer, output) == NOTOK)
	return NOTOK;

    switch (getline (response, sizeof response, input)) {
	case OK: 
	    if (poprint)
		fprintf (stderr, "<--- %s\n", response);
#ifndef NNTP
	    return (*response == '+' ? OK : NOTOK);
#else	/* NNTP */
	    return (*response < CHAR_ERR ? OK : NOTOK);
#endif	/* NNTP */

	case NOTOK: 
	case DONE: 
	    if (poprint)	    
		fprintf (stderr, "%s\n", response);
	    return NOTOK;
    }

    return NOTOK;	/* NOTREACHED */
}


#if defined(MPOP) && !defined(NNTP)
int
multiline (void)
#else
static int
multiline (void)
#endif
{
    char buffer[BUFSIZ + TRMLEN];

    if (getline (buffer, sizeof buffer, input) != OK)
	return NOTOK;
#ifdef DEBUG
    if (poprint)
	fprintf (stderr, "<--- %s\n", response);
#endif DEBUG
    if (strncmp (buffer, TRM, TRMLEN) == 0) {
	if (buffer[TRMLEN] == 0)
	    return DONE;
	else
	    strncpy (response, buffer + TRMLEN, sizeof(response));
    }
    else
	strncpy (response, buffer, sizeof(response));

    return OK;
}


static int
getline (char *s, int n, FILE *iop)
{
    int c;
    char *p;

    p = s;
    while (--n > 0 && (c = fgetc (iop)) != EOF)
	if ((*p++ = c) == '\n')
	    break;
    if (ferror (iop) && c != EOF) {
	strncpy (response, "error on connection", sizeof(response));
	return NOTOK;
    }
    if (c == EOF && p == s) {
	strncpy (response, "connection closed by foreign host", sizeof(response));
	return DONE;
    }
    *p = 0;
    if (*--p == '\n')
	*p = 0;
    if (*--p == '\r')
	*p = 0;

    return OK;
}


static int
putline (char *s, FILE *iop)
{
    fprintf (iop, "%s\r\n", s);
    fflush (iop);
    if (ferror (iop)) {
	strncpy (response, "lost connection", sizeof(response));
	return NOTOK;
    }

    return OK;
}
