/* mf.c -- mail filter subroutines
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mf.h>
#include <h/utils.h>

/*
 * static prototypes
 */
static int isat (const char *);
static int parse_address (void);
static int phrase (char *);
static int route_addr (char *);
static int local_part (char *);
static int domain (char *);
static int route (char *);
static int my_lex (char *);


static int
isat (const char *p)
{
    return *p == ' ' &&
        (p[1] == 'a' || p[1] == 'A') &&
        (p[2] == 't' || p[2] == 'T') &&
        p[3] == ' ';
}


/*
 *
 * getadrx() implements a partial 822-style address parser.  The parser
 * is neither complete nor correct.  It does however recognize nearly all
 * of the 822 address syntax.  In addition it handles the majority of the
 * 733 syntax as well.  Most problems arise from trying to accommodate both.
 *
 * In terms of 822, the route-specification in 
 *
 *		 "<" [route] local-part "@" domain ">"
 *
 * is parsed and returned unchanged.  Multiple at-signs are compressed
 * via source-routing.  Recursive groups are not allowed as per the 
 * standard.
 *
 * In terms of 733, " at " is recognized as equivalent to "@".
 *
 * In terms of both the parser will not complain about missing hosts.
 *
 * -----
 *
 * We should not allow addresses like	
 *
 *		Marshall T. Rose <MRose@UCI>
 *
 * but should insist on
 *
 *		"Marshall T. Rose" <MRose@UCI>
 *
 * Unfortunately, a lot of mailers stupidly let people get away with this.
 *
 * -----
 *
 * We should not allow addresses like
 *
 *		<MRose@UCI>
 *
 * but should insist on
 *
 *		MRose@UCI
 *
 * Unfortunately, a lot of mailers stupidly let people's UAs get away with
 * this.
 *
 * -----
 *
 * We should not allow addresses like
 *
 *		@UCI:MRose@UCI-750a
 *
 * but should insist on
 *
 *		Marshall Rose <@UCI:MRose@UCI-750a>
 *
 * Unfortunately, a lot of mailers stupidly do this.
 *
 */

#define	QUOTE	'\\'

#define	LX_END	 0
#define	LX_ERR	 1
#define	LX_ATOM	 2
#define	LX_QSTR	 3
#define	LX_DLIT	 4
#define	LX_SEMI	 5
#define	LX_COMA	 6
#define	LX_LBRK	 7
#define	LX_RBRK	 8
#define	LX_COLN	 9
#define	LX_DOT	10
#define	LX_AT	11

struct specials {
    char lx_chr;
    int  lx_val;
};

static struct specials special[] = {
    { ';',   LX_SEMI },
    { ',',   LX_COMA },
    { '<',   LX_LBRK },
    { '>',   LX_RBRK },
    { ':',   LX_COLN },
    { '.',   LX_DOT },
    { '@',   LX_AT },
    { '(',   LX_ERR },
    { ')',   LX_ERR },
    { QUOTE, LX_ERR },
    { '"',   LX_ERR },
    { '[',   LX_ERR },
    { ']',   LX_ERR },
    { 0,     0 }
};

static int glevel = 0;
static int ingrp = 0;
static int last_lex = LX_END;

static char *dp = NULL;
static char *cp = NULL;
static char *ap = NULL;
static char *pers = NULL;
static char *mbox = NULL;
static char *host = NULL;
static char *path = NULL;
static char *grp = NULL;
static char *note = NULL;
static char err[BUFSIZ];
static char adr[BUFSIZ];

static struct adrx  adrxs2;


/* eai = Email Address Internationalization */
struct adrx *
getadrx (const char *addrs, int eai)
{
    char *bp;
    struct adrx *adrxp = &adrxs2;

    mh_xfree(pers);
    mh_xfree(mbox);
    mh_xfree(host);
    mh_xfree(path);
    mh_xfree(grp);
    mh_xfree(note);
    pers = mbox = host = path = grp = note = NULL;
    err[0] = 0;

    if (dp == NULL) {
	dp = cp = strdup (addrs ? addrs : "");
	glevel = 0;
    }
    else
	if (cp == NULL) {
	    free (dp);
	    dp = NULL;
	    return NULL;
	}

    switch (parse_address ()) {
	case DONE:
	    free (dp);
	    dp = cp = NULL;
	    return NULL;

	case OK:
	    switch (last_lex) {
		case LX_COMA:
		case LX_END:
		    break;

		default:	/* catch trailing comments */
		    bp = cp;
		    my_lex (adr);
		    cp = bp;
		    break;
	    }
	    break;

	default:
	    break;
	}

    if (! eai) {
        /*
         * Reject the address if key fields contain 8bit characters
         */

        if (contains8bit(mbox, NULL) || contains8bit(host, NULL) ||
            contains8bit(path, NULL) || contains8bit(grp, NULL)) {
            strcpy(err, "Address contains 8-bit characters");
        }
    }

    if (err[0])
	for (;;) {
	    switch (last_lex) {
		case LX_COMA: 
		case LX_END: 
		    break;

		default: 
		    my_lex (adr);
		    continue;
	    }
	    break;
	}
    while (isspace ((unsigned char) *ap))
	ap++;
    if (cp)
	snprintf(adr, sizeof adr, "%.*s", (int)(cp - ap), ap);
    else
	strcpy (adr, ap);
    bp = adr + strlen (adr) - 1;
    if (*bp == ',' || *bp == ';' || *bp == '\n')
	*bp = 0;

    adrxp->text = adr;
    adrxp->pers = pers;
    adrxp->mbox = mbox;
    adrxp->host = host;
    adrxp->path = path;
    adrxp->grp = grp;
    adrxp->ingrp = ingrp;
    adrxp->note = note;
    adrxp->err = err[0] ? err : NULL;

    return adrxp;
}


static int
parse_address (void)
{
    char buffer[BUFSIZ];

again: ;
    ap = cp;
    switch (my_lex (buffer)) {
	case LX_ATOM: 
	case LX_QSTR: 
	    pers = strdup (buffer);
	    break;

	case LX_SEMI: 
	    if (glevel-- <= 0) {
		strcpy (err, "extraneous semi-colon");
		return NOTOK;
	    }
	    /* FALLTHRU */
	case LX_COMA: 
            mh_xfree(note);
            note = NULL;
	    goto again;

	case LX_END: 
	    return DONE;

	case LX_LBRK: 		/* sigh (2) */
	    goto get_addr;

	case LX_AT:		/* sigh (3) */
	    cp = ap;
	    if (route_addr (buffer) == NOTOK)
		return NOTOK;
	    return OK;		/* why be choosy? */

	default: 
	    snprintf(err, sizeof err, "illegal address construct (%s)", buffer);
	    return NOTOK;
    }

    switch (my_lex (buffer)) {
	case LX_ATOM: 
	case LX_QSTR: 
	    pers = add (buffer, add (" ", pers));
    more_phrase: ;		/* sigh (1) */
	    if (phrase (buffer) == NOTOK)
		return NOTOK;

	    switch (last_lex) {
		case LX_LBRK: 
	    get_addr: ;
		    if (route_addr (buffer) == NOTOK)
			return NOTOK;
		    if (last_lex == LX_RBRK)
			return OK;
		    snprintf(err, sizeof err, "missing right-bracket (%s)", buffer);
		    return NOTOK;

		case LX_COLN: 
	    get_group: ;
		    if (glevel++ > 0) {
			snprintf(err, sizeof err, "nested groups not allowed (%s)", pers);
			return NOTOK;
		    }
		    grp = add (": ", pers);
		    pers = NULL;
		    {
			char   *pp = cp;

			for (;;)
			    switch (my_lex (buffer)) {
				case LX_SEMI: 
				case LX_END: /* tsk, tsk */
				    glevel--;
				    return OK;

				case LX_COMA: 
				    continue;

				default: 
				    cp = pp;
				    return parse_address ();
			    }
		    }

		case LX_DOT: 	/* sigh (1) */
		    pers = add (".", pers);
		    goto more_phrase;

		default: 
		    snprintf(err, sizeof err, "no mailbox in address, only a phrase (%s%s)",
			    pers, buffer);
		    return NOTOK;
	    }

	case LX_LBRK: 
	    goto get_addr;

	case LX_COLN: 
	    goto get_group;

	case LX_DOT: 
	    mbox = add (buffer, pers);
	    pers = NULL;
	    if (route_addr (buffer) == NOTOK)
		return NOTOK;
	    goto check_end;

	case LX_AT: 
	    ingrp = glevel;
	    mbox = pers;
	    pers = NULL;
	    if (domain (buffer) == NOTOK)
		return NOTOK;
    check_end: ;
	    switch (last_lex) {
		case LX_SEMI: 
		    if (glevel-- <= 0) {
			strcpy (err, "extraneous semi-colon");
			return NOTOK;
		    }
		    /* FALLTHRU */
		case LX_COMA: 
		case LX_END: 
		    return OK;

		default: 
		    snprintf(err, sizeof err, "junk after local@domain (%s)", buffer);
		    return NOTOK;
	    }

	case LX_SEMI: 		/* no host */
	case LX_COMA: 
	case LX_END: 
	    ingrp = glevel;
	    if (last_lex == LX_SEMI && glevel-- <= 0) {
		strcpy (err, "extraneous semi-colon");
		return NOTOK;
	    }
	    mbox = pers;
	    pers = NULL;
	    return OK;

	default: 
	    snprintf(err, sizeof err, "missing mailbox (%s)", buffer);
	    return NOTOK;
    }
}


static int
phrase (char *buffer)
{
    for (;;)
	switch (my_lex (buffer)) {
	    case LX_ATOM: 
	    case LX_QSTR: 
		pers = add (buffer, add (" ", pers));
		continue;

	    default: 
		return OK;
	}
}


static int
route_addr (char *buffer)
{
    char *pp = cp;

    if (my_lex (buffer) == LX_AT) {
	if (route (buffer) == NOTOK)
	    return NOTOK;
    }
    else
	cp = pp;

    if (local_part (buffer) == NOTOK)
	return NOTOK;

    switch (last_lex) {
	case LX_AT: 
	    return domain (buffer);

	case LX_SEMI:	/* if in group */
	case LX_RBRK: 		/* no host */
	case LX_COMA:
	case LX_END: 
	    return OK;

	default: 
	    snprintf(err, sizeof err, "no at-sign after local-part (%s)", buffer);
	    return NOTOK;
    }
}


static int
local_part (char *buffer)
{
    ingrp = glevel;

    for (;;) {
	switch (my_lex (buffer)) {
	    case LX_ATOM: 
	    case LX_QSTR: 
		mbox = add (buffer, mbox);
		break;

	    default: 
		snprintf(err, sizeof err, "no mailbox in local-part (%s)", buffer);
		return NOTOK;
	}

	switch (my_lex (buffer)) {
	    case LX_DOT: 
		mbox = add (buffer, mbox);
		continue;

	    default: 
		return OK;
	}
    }
}


static int
domain (char *buffer)
{
    for (;;) {
	switch (my_lex (buffer)) {
	    case LX_ATOM: 
	    case LX_DLIT: 
		host = add (buffer, host);
		break;

	    default: 
		snprintf(err, sizeof err, "no sub-domain in domain-part of address (%s)", buffer);
		return NOTOK;
	}

	switch (my_lex (buffer)) {
	    case LX_DOT: 
		host = add (buffer, host);
		continue;

	    case LX_AT: 	/* sigh (0) */
		mbox = add (host, add ("%", mbox));
		free (host);
		host = NULL;
		continue;

	    default: 
		return OK;
	}
    }
}


static int
route (char *buffer)
{
    path = mh_xstrdup ("@");

    for (;;) {
	switch (my_lex (buffer)) {
	    case LX_ATOM: 
	    case LX_DLIT: 
		path = add (buffer, path);
		break;

	    default: 
		snprintf(err, sizeof err, "no sub-domain in domain-part of address (%s)", buffer);
		return NOTOK;
	}
	switch (my_lex (buffer)) {
	    case LX_COMA: 
		path = add (buffer, path);
		for (;;) {
		    switch (my_lex (buffer)) {
			case LX_COMA: 
			    continue;

			case LX_AT: 
			    path = add (buffer, path);
			    break;

			default: 
			    snprintf(err, sizeof err, "no at-sign found for next domain in route (%s)",
			             buffer);
		    }
		    break;
		}
		continue;

	    case LX_AT:		/* XXX */
	    case LX_DOT: 
		path = add (buffer, path);
		continue;

	    case LX_COLN: 
		path = add (buffer, path);
		return OK;

	    default: 
		snprintf(err, sizeof err, "no colon found to terminate route (%s)", buffer);
		return NOTOK;
	}
    }
}


static int
my_lex (char *buffer)
{
    /* buffer should be at least BUFSIZ bytes long */
    int i, gotat = 0;
    char c, *bp;

/* Add C to the buffer bp. After use of this macro *bp is guaranteed to be within the buffer. */
#define ADDCHR(C) do { *bp++ = (C); if ((bp - buffer) == (BUFSIZ-1)) goto my_lex_buffull; } while (0)

    bp = buffer;
    *bp = 0;
    if (!cp)
	return (last_lex = LX_END);

    gotat = isat (cp);
    c = *cp++;
    while (isspace ((unsigned char) c))
	c = *cp++;
    if (c == 0) {
	cp = NULL;
	return (last_lex = LX_END);
    }

    if (c == '(') {
	ADDCHR(c);
	for (i = 0;;)
	    switch (c = *cp++) {
		case 0: 
		    cp = NULL;
		    return (last_lex = LX_ERR);
		case QUOTE: 
		    ADDCHR(c);
		    if ((c = *cp++) == 0) {
			cp = NULL;
			return (last_lex = LX_ERR);
		    }
		    ADDCHR(c);
		    continue;
		case '(': 
		    i++;
		    /* FALLTHRU */
		default: 
		    ADDCHR(c);
		    continue;
		case ')': 
		    ADDCHR(c);
		    if (--i < 0) {
			*bp = 0;
			note = note ? add (buffer, add (" ", note))
			    : strdup (buffer);
			return my_lex (buffer);
		    }
	    }
    }

    if (c == '"') {
	ADDCHR(c);
	for (;;)
	    switch (c = *cp++) {
		case 0: 
		    cp = NULL;
		    return (last_lex = LX_ERR);
		case QUOTE: 
		    ADDCHR(c);
		    if ((c = *cp++) == 0) {
			cp = NULL;
			return (last_lex = LX_ERR);
		    }
		    /* FALLTHRU */
		default: 
		    ADDCHR(c);
		    continue;
		case '"': 
		    ADDCHR(c);
		    *bp = 0;
		    return (last_lex = LX_QSTR);
	    }
    }
    
    if (c == '[') {
	ADDCHR(c);
	for (;;)
	    switch (c = *cp++) {
		case 0: 
		    cp = NULL;
		    return (last_lex = LX_ERR);
		case QUOTE: 
		    ADDCHR(c);
		    if ((c = *cp++) == 0) {
			cp = NULL;
			return (last_lex = LX_ERR);
		    }
		    /* FALLTHRU */
		default: 
		    ADDCHR(c);
		    continue;
		case ']': 
		    ADDCHR(c);
		    *bp = 0;
		    return (last_lex = LX_DLIT);
	    }
    }
    
    ADDCHR(c);
    *bp = 0;
    for (i = 0; special[i].lx_chr != 0; i++)
	if (c == special[i].lx_chr)
	    return (last_lex = special[i].lx_val);

    if (iscntrl ((unsigned char) c))
	return (last_lex = LX_ERR);

    for (;;) {
	if ((c = *cp++) == 0)
	    break;
	for (i = 0; special[i].lx_chr != 0; i++)
	    if (c == special[i].lx_chr)
		goto got_atom;
	if (iscntrl ((unsigned char) c) || isspace ((unsigned char) c))
	    break;
	ADDCHR(c);
    }
got_atom: ;
    if (c == 0)
	cp = NULL;
    else
	cp--;
    *bp = 0;
    last_lex = !gotat || cp == NULL || strchr(cp, '<') != NULL
	? LX_ATOM : LX_AT;
    return last_lex;

 my_lex_buffull:
    /* Out of buffer space. *bp is the last byte in the buffer */
    *bp = 0;
    return (last_lex = LX_ERR);
}


char *
legal_person (const char *p)
{
    int i;
    const char *cp;
    static char buffer[BUFSIZ];

    if (*p == '"')
	return (char *) p;
    for (cp = p; *cp; cp++)
	for (i = 0; special[i].lx_chr; i++)
	    if (*cp == special[i].lx_chr) {
		snprintf(buffer, sizeof buffer, "\"%s\"", p);
		return buffer;
	    }

    return (char *) p;
}
