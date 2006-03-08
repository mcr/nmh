
/*
 * sortm.c -- sort messages in a folder by date/time
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/tws.h>
#include <h/utils.h>

/*
 * We allocate space for messages (msgs array)
 * this number of elements at a time.
 */
#define MAXMSGS  256


static struct swit switches[] = {
#define DATESW                 0
     { "datefield field", 0 },
#define	TEXTSW                 1
     { "textfield field", 0 },
#define	NSUBJSW                2
     { "notextfield", 0 },
#define SUBJSW                 3
     { "subject", -3 },		   /* backward-compatibility */
#define LIMSW                  4
     { "limit days", 0 },
#define	NLIMSW                 5
     { "nolimit", 0 },
#define VERBSW                 6
     { "verbose", 0 },
#define NVERBSW                7
     { "noverbose", 0 },
#define VERSIONSW              8
     { "version", 0 },
#define HELPSW                 9
     { "help", 0 },
     { NULL, 0 }
};

struct smsg {
    int s_msg;
    time_t s_clock;
    char *s_subj;
};

static struct smsg *smsgs;
int nmsgs;

char *subjsort = (char *) 0;    /* sort on subject if != 0 */
unsigned long datelimit = 0;
int submajor = 0;		/* if true, sort on subject-major */
int verbose;

/* This keeps compiler happy on calls to qsort */
typedef int (*qsort_comp) (const void *, const void *);

/*
 * static prototypes
 */
static int read_hdrs (struct msgs *, char *);
static int get_fields (char *, int, struct smsg *);
static int dsort (struct smsg **, struct smsg **);
static int subsort (struct smsg **, struct smsg **);
static int txtsort (struct smsg **, struct smsg **);
static void rename_chain (struct msgs *, struct smsg **, int, int);
static void rename_msgs (struct msgs *, struct smsg **);


int
main (int argc, char **argv)
{
    int	nummsgs, maxmsgs, i, msgnum;
    char *cp, *maildir, *datesw = NULL;
    char *folder = NULL, buf[BUFSIZ], **argp;
    char **arguments, **msgs;
    struct msgs *mp;
    struct smsg **dlist;

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
    msgs = (char **) mh_xmalloc ((size_t) (maxmsgs * sizeof(*msgs)));

    /*
     * Parse arguments
     */
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW:
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW:
		adios (NULL, "-%s unknown", cp);

	    case HELPSW:
		snprintf(buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		print_help (buf, switches, 1);
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

	    case DATESW:
		if (datesw)
		    adios (NULL, "only one date field at a time");
		if (!(datesw = *argp++) || *datesw == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case TEXTSW:
		if (subjsort)
		    adios (NULL, "only one text field at a time");
		if (!(subjsort = *argp++) || *subjsort == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case SUBJSW:
		subjsort = "subject";
		continue;
	    case NSUBJSW:
		subjsort = (char *)0;
		continue;

	    case LIMSW:
		if (!(cp = *argp++) || *cp == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		while (*cp == '0')
		    cp++;		/* skip any leading zeros */
		if (!*cp) {		/* hit end of string */
		    submajor++;		/* sort subject-major */
		    continue;
		}
		if (!isdigit(*cp) || !(datelimit = atoi(cp)))
		    adios (NULL, "impossible limit %s", cp);
		datelimit *= 60*60*24;
		continue;
	    case NLIMSW:
		submajor = 0;	/* use date-major, but */
		datelimit = 0;	/* use no limit */
		continue;

	    case VERBSW:
		verbose++;
		continue;
	    case NVERBSW:
		verbose = 0;
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
	     * for message names/ranges.
	     */
	    if (nummsgs >= maxmsgs) {
		maxmsgs += MAXMSGS;
		msgs = (char **) mh_xrealloc (msgs,
                    (size_t) (maxmsgs * sizeof(*msgs)));
	    }
	    msgs[nummsgs++] = cp;
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!nummsgs)
	msgs[nummsgs++] = "all";
    if (!datesw)
	datesw = "date";
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
    seq_setprev (mp);	/* set the previous sequence */

    if ((nmsgs = read_hdrs (mp, datesw)) <= 0)
	adios (NULL, "no messages to sort");

    /*
     * sort a list of pointers to our "messages to be sorted".
     */
    dlist = (struct smsg **) mh_xmalloc ((nmsgs+1) * sizeof(*dlist));
    for (i = 0; i < nmsgs; i++)
	dlist[i] = &smsgs[i];
    dlist[nmsgs] = 0;

    if (verbose) {	/* announce what we're doing */
	if (subjsort)
	    printf ("sorting by %s-major %s-minor\n",
		submajor ? subjsort : datesw,
		submajor ? datesw : subjsort);
	else
	    printf ("sorting by datefield %s\n", datesw);
    }

    /* first sort by date, or by subject-major, date-minor */
    qsort ((char *) dlist, nmsgs, sizeof(*dlist),
	    (qsort_comp) (submajor && subjsort ? txtsort : dsort));

    /*
     * if we're sorting on subject, we need another list
     * in subject order, then a merge pass to collate the
     * two sorts.
     */
    if (!submajor && subjsort) {	/* already date sorted */
	struct smsg **slist, **flist;
	register struct smsg ***il, **fp, **dp;

	slist = (struct smsg **) mh_xmalloc ((nmsgs+1) * sizeof(*slist));
	memcpy((char *)slist, (char *)dlist, (nmsgs+1)*sizeof(*slist));
	qsort((char *)slist, nmsgs, sizeof(*slist), (qsort_comp) subsort);

	/*
	 * make an inversion list so we can quickly find
	 * the collection of messages with the same subj
	 * given a message number.
	 */
	il = (struct smsg ***) calloc (mp->hghsel+1, sizeof(*il));
	if (! il)
	    adios (NULL, "couldn't allocate msg list");
	for (i = 0; i < nmsgs; i++)
	    il[slist[i]->s_msg] = &slist[i];
	/*
	 * make up the final list, chronological but with
	 * all the same subjects grouped together.
	 */
	flist = (struct smsg **) mh_xmalloc ((nmsgs+1) * sizeof(*flist));
	fp = flist;
	for (dp = dlist; *dp;) {
	    register struct smsg **s = il[(*dp++)->s_msg];

	    /* see if we already did this guy */
	    if (! s)
		continue;

	    *fp++ = *s++;
	    /*
	     * take the next message(s) if there is one,
	     * its subject isn't null and its subject
	     * is the same as this one and it's not too
	     * far away in time.
	     */
	    while (*s && (*s)->s_subj[0] &&
		   strcmp((*s)->s_subj, s[-1]->s_subj) == 0 &&
		   (datelimit == 0 ||
		   (*s)->s_clock - s[-1]->s_clock <= datelimit)) {
		il[(*s)->s_msg] = 0;
		*fp++ = *s++;
	    }
	}
	*fp = 0;
	free (slist);
	free (dlist);
	dlist = flist;
    }

    /*
     * At this point, dlist is a sorted array of pointers to smsg structures,
     * each of which contains a message number.
     */

    rename_msgs (mp, dlist);

    context_replace (pfolder, folder);	/* update current folder         */
    seq_save (mp);			/* synchronize message sequences */
    context_save ();			/* save the context file         */
    folder_free (mp);			/* free folder/message structure */
    return done (0);
}

static int
read_hdrs (struct msgs *mp, char *datesw)
{
    int msgnum;
    struct tws tb;
    register struct smsg *s;

    twscopy (&tb, dlocaltimenow ());

    smsgs = (struct smsg *)
	calloc ((size_t) (mp->hghsel - mp->lowsel + 2),
	    sizeof(*smsgs));
    if (smsgs == NULL)
	adios (NULL, "unable to allocate sort storage");

    s = smsgs;
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected(mp, msgnum)) {
	    if (get_fields (datesw, msgnum, s)) {
		s->s_msg = msgnum;
		s++;
	    }
	}
    }
    s->s_msg = 0;
    return(s - smsgs);
}


/*
 * Parse the message and get the data or subject field,
 * if needed.
 */

static int
get_fields (char *datesw, int msg, struct smsg *smsg)
{
    register int state;
    int compnum;
    char *msgnam, buf[BUFSIZ], nam[NAMESZ];
    register struct tws *tw;
    register char *datecomp = NULL, *subjcomp = NULL;
    register FILE *in;

    if ((in = fopen (msgnam = m_name (msg), "r")) == NULL) {
	admonish (msgnam, "unable to read message");
	return (0);
    }
    for (compnum = 1, state = FLD;;) {
	switch (state = m_getfld (state, nam, buf, sizeof(buf), in)) {
	case FLD:
	case FLDEOF:
	case FLDPLUS:
	    compnum++;
	    if (!mh_strcasecmp (nam, datesw)) {
		datecomp = add (buf, datecomp);
		while (state == FLDPLUS) {
		    state = m_getfld (state, nam, buf, sizeof(buf), in);
		    datecomp = add (buf, datecomp);
		}
		if (!subjsort || subjcomp)
		    break;
	    } else if (subjsort && !mh_strcasecmp (nam, subjsort)) {
		subjcomp = add (buf, subjcomp);
		while (state == FLDPLUS) {
		    state = m_getfld (state, nam, buf, sizeof(buf), in);
		    subjcomp = add (buf, subjcomp);
		}
		if (datecomp)
		    break;
	    } else {
		/* just flush this guy */
		while (state == FLDPLUS)
		    state = m_getfld (state, nam, buf, sizeof(buf), in);
	    }
	    continue;

	case BODY:
	case BODYEOF:
	case FILEEOF:
	    break;

	case LENERR:
	case FMTERR:
	    if (state == LENERR || state == FMTERR)
		admonish (NULL, "format error in message %d (header #%d)",
		      msg, compnum);
	    if (datecomp)
		free (datecomp);
	    if (subjcomp)
		free (subjcomp);
	    fclose (in);
	    return (0);

	default:
	    adios (NULL, "internal error -- you lose");
	}
	break;
    }

    /*
     * If no date component, then use the modification
     * time of the file as its date
     */
    if (!datecomp || (tw = dparsetime (datecomp)) == NULL) {
	struct stat st;

	admonish (NULL, "can't parse %s field in message %d", datesw, msg);
	fstat (fileno (in), &st);
	smsg->s_clock = st.st_mtime;
    } else {
	smsg->s_clock = dmktime (tw);
    }

    if (subjsort) {
	if (subjcomp) {
	    /*
	     * try to make the subject "canonical": delete
	     * leading "re:", everything but letters & smash
	     * letters to lower case.
	     */
	    register char  *cp, *cp2, c;

	    cp = subjcomp;
	    cp2 = subjcomp;
	    if (strcmp (subjsort, "subject") == 0) {
		while ((c = *cp)) {
		    if (! isspace(c)) {
			if(uprf(cp, "re:"))
			    cp += 2;
			else
			    break;
		    }
		    cp++;
		}
	    }

	    while ((c = *cp++)) {
		if (isalnum(c))
		    *cp2++ = isupper(c) ? tolower(c) : c;
	    }

	    *cp2 = '\0';
	}
	else
	    subjcomp = "";

	smsg->s_subj = subjcomp;
    }
    fclose (in);
    if (datecomp)
	free (datecomp);

    return (1);
}

/*
 * sort on dates.
 */
static int
dsort (struct smsg **a, struct smsg **b)
{
    if ((*a)->s_clock < (*b)->s_clock)
	return (-1);
    else if ((*a)->s_clock > (*b)->s_clock)
	return (1);
    else if ((*a)->s_msg < (*b)->s_msg)
	return (-1);
    else
	return (1);
}

/*
 * sort on subjects.
 */
static int
subsort (struct smsg **a, struct smsg **b)
{
    register int i;

    if ((i = strcmp ((*a)->s_subj, (*b)->s_subj)))
	return (i);

    return (dsort (a, b));
}

static int
txtsort (struct smsg **a, struct smsg **b)
{
    register int i;

    if ((i = strcmp ((*a)->s_subj, (*b)->s_subj)))
	return (i);
    else if ((*a)->s_msg < (*b)->s_msg)
	return (-1);
    else
	return (1);
}

static void
rename_chain (struct msgs *mp, struct smsg **mlist, int msg, int endmsg)
{
    int nxt, old, new;
    char *newname, oldname[BUFSIZ];
    char newbuf[MAXPATHLEN + 1];

    for (;;) {
	nxt = mlist[msg] - smsgs;	/* mlist[msg] is a ptr into smsgs */
	mlist[msg] = (struct smsg *)0;
	old = smsgs[nxt].s_msg;
	new = smsgs[msg].s_msg;
	strncpy (oldname, m_name (old), sizeof(oldname));
	newname = m_name (new);
	if (verbose)
	    printf ("message %d becomes message %d\n", old, new);

	(void)snprintf(oldname, sizeof (oldname), "%s/%d", mp->foldpath, old);
	(void)snprintf(newbuf, sizeof (newbuf), "%s/%d", mp->foldpath, new);
	ext_hook("ref-hook", oldname, newbuf);

	if (rename (oldname, newname) == NOTOK)
	    adios (newname, "unable to rename %s to", oldname);

	copy_msg_flags (mp, new, old);
	if (mp->curmsg == old)
	    seq_setcur (mp, new);

	if (nxt == endmsg)
	    break;

	msg = nxt;
    }
/*	if (nxt != endmsg); */
/*	rename_chain (mp, mlist, nxt, endmsg); */
}

static void
rename_msgs (struct msgs *mp, struct smsg **mlist)
{
    int i, j, old, new;
    seqset_t tmpset;
    char f1[BUFSIZ], tmpfil[BUFSIZ];
    char newbuf[MAXPATHLEN + 1];
    struct smsg *sp;

    strncpy (tmpfil, m_name (mp->hghmsg + 1), sizeof(tmpfil));

    for (i = 0; i < nmsgs; i++) {
	if (! (sp = mlist[i]))
	    continue;   /* did this one */

	j = sp - smsgs;
	if (j == i)
	    continue;   /* this one doesn't move */

	/*
	 * the guy that was msg j is about to become msg i.
	 * rename 'j' to make a hole, then recursively rename
	 * guys to fill up the hole.
	 */
	old = smsgs[j].s_msg;
	new = smsgs[i].s_msg;
	strncpy (f1, m_name (old), sizeof(f1));

	if (verbose)
	    printf ("renaming message chain from %d to %d\n", old, new);

	/*
	 *	Run the external hook to refile the old message as the
	 *	temporary message number that is off of the end of the
	 *	messages in the folder.
	 */

	(void)snprintf(f1, sizeof (f1), "%s/%d", mp->foldpath, old);
	(void)snprintf(newbuf, sizeof (newbuf), "%s/%d", mp->foldpath, mp->hghmsg + 1);
	ext_hook("ref-hook", f1, newbuf);

	if (rename (f1, tmpfil) == NOTOK)
	    adios (tmpfil, "unable to rename %s to ", f1);

	get_msg_flags (mp, &tmpset, old);

	rename_chain (mp, mlist, j, i);

	/*
	 *	Run the external hook to refile the temorary message number
	 *	to the real place.
	 */

	(void)snprintf(f1, sizeof (f1), "%s/%d", mp->foldpath, new);
	ext_hook("ref-hook", newbuf, f1);

	if (rename (tmpfil, m_name(new)) == NOTOK)
	    adios (m_name(new), "unable to rename %s to", tmpfil);

	set_msg_flags (mp, &tmpset, new);
	mp->msgflags |= SEQMOD;
    }
}
