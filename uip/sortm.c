/* sortm.c -- sort messages in a folder by date/time
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/tws.h>
#include "h/done.h"
#include <h/utils.h>
#include "sbr/m_maildir.h"

#define SORTM_SWITCHES \
    X("datefield field", 0, DATESW) \
    X("textfield field", 0, TEXTSW) \
    X("notextfield", 0, NSUBJSW) \
    X("subject", -3, SUBJSW) /* backward-compatibility */ \
    X("limit days", 0, LIMSW) \
    X("nolimit", 0, NLIMSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("all", 0, ALLMSGS) \
    X("noall", 0, NALLMSGS) \
    X("check", 0, CHECKSW) \
    X("nocheck", 0, NCHECKSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SORTM);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SORTM, switches);
#undef X

struct smsg {
    int s_msg;
    time_t s_clock;
    char *s_subj;
};

static struct smsg *smsgs;
int nmsgs;

char *subjsort;                 /* sort on subject if != 0 */
time_t datelimit = 0;
int submajor = 0;		/* if true, sort on subject-major */
int verbose;
int allmsgs = 1;
int check_failed = 0;

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
    int	i, msgnum;
    char *cp, *maildir, *datesw = NULL;
    char *folder = NULL, buf[BUFSIZ], **argp;
    char **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp;
    struct smsg **dlist;
    int checksw = 0;

    if (nmh_init(argv[0], 1)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

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
		die("-%s unknown", cp);

	    case HELPSW:
		snprintf(buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case DATESW:
		if (datesw)
		    die("only one date field at a time");
		if (!(datesw = *argp++) || *datesw == '-')
		    die("missing argument to %s", argp[-2]);
		continue;

	    case TEXTSW:
		if (subjsort)
		    die("only one text field at a time");
		if (!(subjsort = *argp++) || *subjsort == '-')
		    die("missing argument to %s", argp[-2]);
		continue;

	    case SUBJSW:
		subjsort = "subject";
		continue;
	    case NSUBJSW:
		subjsort = NULL;
		continue;

	    case LIMSW:
		if (!(cp = *argp++) || *cp == '-')
			die("missing argument to %s", argp[-2]);
		while (*cp == '0')
		    cp++;		/* skip any leading zeros */
		if (!*cp) {		/* hit end of string */
		    submajor++;		/* sort subject-major */
		    continue;
		}
		if (!isdigit((unsigned char) *cp) || !(datelimit = atoi(cp)))
		    die("impossible limit %s", cp);
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

	    case ALLMSGS:
		allmsgs = 1;
		continue;
	    case NALLMSGS:
		allmsgs = 0;
		continue;

	    case CHECKSW:
		checksw = 1;
		continue;
	    case NCHECKSW:
		checksw = 0;
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		die("only one folder at a time!");
            folder = pluspath (cp);
	} else
            app_msgarg(&msgs, cp);
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!msgs.size) {
	if (allmsgs) {
	    app_msgarg(&msgs, "all");
        } else {
	    die("must specify messages to sort with -noall");
        }
    }
    if (!datesw)
	datesw = "date";
    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder, 1)))
	die("unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	die("no messages in %s", folder);

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous sequence */

    if ((nmsgs = read_hdrs (mp, datesw)) <= 0)
	die("no messages to sort");

    if (checksw  &&  check_failed) {
	die("errors found, no messages sorted");
    }

    /*
     * sort a list of pointers to our "messages to be sorted".
     */
    dlist = mh_xmalloc ((nmsgs+1) * sizeof(*dlist));
    for (i = 0; i < nmsgs; i++)
	dlist[i] = &smsgs[i];
    dlist[nmsgs] = 0;

    if (verbose) {	/* announce what we're doing */
	if (subjsort)
	    if (submajor)
		printf ("sorting by %s\n", subjsort);
	    else
		printf ("sorting by %s-major %s-minor\n", subjsort, datesw);
	else
	    printf ("sorting by datefield %s\n", datesw);
    }

    /* first sort by date, or by subject-major, date-minor */
    qsort (dlist, nmsgs, sizeof(*dlist),
	    (qsort_comp) (submajor && subjsort ? txtsort : dsort));

    /*
     * if we're sorting on subject, we need another list
     * in subject order, then a merge pass to collate the
     * two sorts.
     */
    if (!submajor && subjsort) {	/* already date sorted */
	struct smsg **slist, **flist;
	struct smsg ***il, **fp, **dp;

	slist = mh_xmalloc ((nmsgs+1) * sizeof(*slist));
	memcpy(slist, dlist, (nmsgs+1)*sizeof(*slist));
	qsort(slist, nmsgs, sizeof(*slist), (qsort_comp) subsort);

	/*
	 * make an inversion list so we can quickly find
	 * the collection of messages with the same subj
	 * given a message number.
	 */
	il = mh_xcalloc(mp->hghsel + 1, sizeof *il);
	for (i = 0; i < nmsgs; i++)
	    il[slist[i]->s_msg] = &slist[i];
	/*
	 * make up the final list, chronological but with
	 * all the same subjects grouped together.
	 */
	flist = mh_xmalloc ((nmsgs+1) * sizeof(*flist));
	fp = flist;
	for (dp = dlist; *dp;) {
	    struct smsg **s = il[(*dp++)->s_msg];

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
	free (il);
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
    done (0);
    return 1;
}

static int
read_hdrs (struct msgs *mp, char *datesw)
{
    int msgnum;
    struct smsg *s;

    smsgs = mh_xcalloc(mp->hghsel - mp->lowsel + 2, sizeof *smsgs);
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
    return s - smsgs;
}


/*
 * Parse the message and get the data or subject field,
 * if needed.
 */

static int
get_fields (char *datesw, int msg, struct smsg *smsg)
{
    int state;
    int compnum;
    char *msgnam, buf[NMH_BUFSIZ], nam[NAMESZ];
    struct tws *tw;
    char *datecomp = NULL, *subjcomp = NULL;
    FILE *in;
    m_getfld_state_t gstate;

    if ((in = fopen (msgnam = m_name (msg), "r")) == NULL) {
	admonish (msgnam, "unable to read message");
	return 0;
    }
    gstate = m_getfld_state_init(in);
    for (compnum = 1;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld2(&gstate, nam, buf, &bufsz)) {
	case FLD:
	case FLDPLUS:
	    compnum++;
	    if (!strcasecmp (nam, datesw)) {
		datecomp = add (buf, datecomp);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld2(&gstate, nam, buf, &bufsz);
		    datecomp = add (buf, datecomp);
		}
		if (!subjsort || subjcomp)
		    break;
	    } else if (subjsort && !strcasecmp (nam, subjsort)) {
		subjcomp = add (buf, subjcomp);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld2(&gstate, nam, buf, &bufsz);
		    subjcomp = add (buf, subjcomp);
		}
		if (datecomp)
		    break;
	    } else {
		/* just flush this guy */
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld2(&gstate, nam, buf, &bufsz);
		}
	    }
	    continue;

	case BODY:
	case FILEEOF:
	    break;

	case LENERR:
	case FMTERR:
	    if (state == LENERR || state == FMTERR) {
		inform("format error in message %d (header #%d), continuing...",
		      msg, compnum);
		check_failed = 1;
	    }
            free(datecomp);
            free(subjcomp);
	    fclose (in);
	    return 0;

	default:
	    die("internal error -- you lose");
	}
	break;
    }
    m_getfld_state_destroy (&gstate);

    /*
     * If no date component, then use the modification
     * time of the file as its date
     */
    if (!datecomp || (tw = dparsetime (datecomp)) == NULL) {
	struct stat st;

	inform("can't parse %s field in message %d, "
            "will use file modification time", datesw, msg);
	fstat (fileno (in), &st);
	smsg->s_clock = st.st_mtime;
	check_failed = 1;
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
	    char  *cp, *cp2, c;

	    cp = subjcomp;
	    cp2 = subjcomp;
	    if (strcmp (subjsort, "subject") == 0) {
		while ((c = *cp)) {
		    if (! isspace((unsigned char) c)) {
			if(!uprf(cp, "re:"))
			    break;
                        cp += 2;
		    }
		    cp++;
		}
	    }

	    while ((c = *cp++)) {
		if (isascii((unsigned char) c) && isalnum((unsigned char) c))
		    *cp2++ = tolower((unsigned char)c);
	    }

	    *cp2 = '\0';
	}
	else
	    subjcomp = "";

	smsg->s_subj = subjcomp;
    }
    fclose (in);
    free(datecomp);

    return 1;
}

/*
 * sort on dates.
 */
static int
dsort (struct smsg **a, struct smsg **b)
{
    if ((*a)->s_clock < (*b)->s_clock)
	return -1;
    if ((*a)->s_clock > (*b)->s_clock)
	return 1;
    if ((*a)->s_msg < (*b)->s_msg)
	return -1;
    return 1;
}

/*
 * sort on subjects.
 */
static int
subsort (struct smsg **a, struct smsg **b)
{
    int i;

    if ((i = strcmp ((*a)->s_subj, (*b)->s_subj)))
	return i;

    return dsort(a, b);
}

static int
txtsort (struct smsg **a, struct smsg **b)
{
    int i;

    if ((i = strcmp ((*a)->s_subj, (*b)->s_subj)))
	return i;
    if ((*a)->s_msg < (*b)->s_msg)
	return -1;
    return 1;
}

static void
rename_chain (struct msgs *mp, struct smsg **mlist, int msg, int endmsg)
{
    int nxt, old, new;
    char *newname, oldname[BUFSIZ];
    char newbuf[PATH_MAX + 1];

    for (;;) {
	nxt = mlist[msg] - smsgs;	/* mlist[msg] is a ptr into smsgs */
	mlist[msg] = NULL;
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
    bvector_t tmpset = bvector_create ();
    char f1[BUFSIZ], tmpfil[BUFSIZ];
    char newbuf[PATH_MAX + 1];
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

	get_msg_flags (mp, tmpset, old);

	rename_chain (mp, mlist, j, i);

	/*
	 *	Run the external hook to refile the temporary message number
	 *	to the real place.
	 */

	(void)snprintf(f1, sizeof (f1), "%s/%d", mp->foldpath, new);
	ext_hook("ref-hook", newbuf, f1);

	if (rename (tmpfil, m_name(new)) == NOTOK)
	    adios (m_name(new), "unable to rename %s to", tmpfil);

	set_msg_flags (mp, tmpset, new);
	mp->msgflags |= SEQMOD;
    }

    bvector_free (tmpset);
}
