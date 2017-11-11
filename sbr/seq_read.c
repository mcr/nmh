/* seq_read.c -- read the .mh_sequence file and
 *            -- initialize sequence information
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "getcpy.h"
#include "m_atoi.h"
#include "brkstring.h"
#include "error.h"
#include "h/utils.h"
#include "lock_file.h"

/*
 * static prototypes
 */
static int seq_init (struct msgs *, char *, char *);
static int seq_public (struct msgs *, int, int *);
static void seq_private (struct msgs *);


/*
 * Get the sequence information for this folder from
 * .mh_sequences (or equivalent specified in .mh_profile)
 * or context file (for private sequences).
 */

int
seq_read (struct msgs *mp, int lockflag)
{
    int failed_to_lock = 0;

    /*
     * Initialize the list of sequence names.  Go ahead and
     * add the "cur" sequence to the list of sequences.
     */
    svector_push_back (mp->msgattrs, getcpy (current));
    make_all_public (mp);	/* initially, make all public */

    /* If folder is empty, don't scan for sequence information */
    if (mp->nummsg == 0)
	return OK;

    /* Initialize the public sequences */
    if (seq_public (mp, lockflag, &failed_to_lock) == NOTOK) {
	if (failed_to_lock) return NOTOK;
    }

    /* Initialize the private sequences */
    seq_private (mp);

    return OK;
}


/*
 * read folder's sequences file for public sequences
 */

static int
seq_public (struct msgs *mp, int lockflag, int *failed_to_lock)
{
    int state;
    char *cp, seqfile[PATH_MAX];
    char name[NAMESZ], field[NMH_BUFSIZ];
    FILE *fp;
    m_getfld_state_t gstate;

    /*
     * If mh_seq == NULL or if *mh_seq == '\0' (the user has defined
     * the "mh-sequences" profile entry, but left it empty),
     * then just return, and do not initialize any public sequences.
     */
    if (mh_seq == NULL || *mh_seq == '\0')
	return OK;

    /* get filename of sequence file */
    snprintf (seqfile, sizeof(seqfile), "%s/%s", mp->foldpath, mh_seq);

    if ((fp = lkfopendata (seqfile, lockflag ? "r+" : "r", failed_to_lock))
	== NULL)
	return NOTOK;

    /* Use m_getfld2 to scan sequence file */
    gstate = m_getfld_state_init(fp);
    for (;;) {
	int fieldsz = sizeof field;
	switch (state = m_getfld2(&gstate, name, field, &fieldsz)) {
	    case FLD: 
	    case FLDPLUS:
		if (state == FLDPLUS) {
		    cp = mh_xstrdup(field);
		    while (state == FLDPLUS) {
			fieldsz = sizeof field;
			state = m_getfld2(&gstate, name, field, &fieldsz);
			cp = add (field, cp);
		    }
		    seq_init (mp, mh_xstrdup(name), trimcpy (cp));
		    free (cp);
		} else {
		    seq_init (mp, mh_xstrdup(name), trimcpy (field));
		}
		continue;

	    case BODY:
	    	lkfclosedata (fp, seqfile);
		die("no blank lines are permitted in %s", seqfile);
		break;

	    case FILEEOF:
		break;

	    default: 
	    	lkfclosedata (fp, seqfile);
		die("%s is poorly formatted", seqfile);
	}
	break;	/* break from for loop */
    }
    m_getfld_state_destroy (&gstate);

    if (lockflag) {
	mp->seqhandle = fp;
	mp->seqname = mh_xstrdup(seqfile);
    } else {
	lkfclosedata (fp, seqfile);
    }

    return OK;
}


/*
 * Scan profile/context list for private sequences.
 *
 * We search the context list for all keys that look like
 * "atr-seqname-folderpath", and add them as private sequences.
 */

static void
seq_private (struct msgs *mp)
{
    int i, j, alen, plen;
    char *cp;
    struct node *np;

    alen = LEN("atr-");
    plen = strlen (mp->foldpath) + 1;

    for (np = m_defs; np; np = np->n_next) {
	if (ssequal ("atr-", np->n_name)
		&& (j = strlen (np->n_name) - plen) > alen
		&& *(np->n_name + j) == '-'
		&& strcmp (mp->foldpath, np->n_name + j + 1) == 0) {
	    cp = mh_xstrdup(np->n_name + alen);
	    *(cp + j - alen) = '\0';
	    if ((i = seq_init (mp, cp, getcpy (np->n_field))) != -1)
		make_seq_private (mp, i);
	}
    }
}


/*
 * Add the name of sequence to the list of folder sequences.
 * Then parse the list of message ranges for this
 * sequence, and setup the various bit flags for each
 * message in the sequence.
 *
 * Return internal index for the sequence if successful.
 * Return -1 on error.
 */

static int
seq_init (struct msgs *mp, char *name, char *field)
{
    unsigned int i;
    int j, k, is_current;
    char *cp, **ap;

    /*
     * Check if this is "cur" sequence,
     * so we can do some special things.
     */
    is_current = !strcmp (current, name);

    /*
     * Search for this sequence name to see if we've seen
     * it already.  If we've seen this sequence before,
     * then clear the bit for this sequence from all the
     * messages in this folder.
     */
    for (i = 0; i < svector_size (mp->msgattrs); i++) {
	if (!strcmp (svector_at (mp->msgattrs, i), name)) {
	    for (j = mp->lowmsg; j <= mp->hghmsg; j++)
		clear_sequence (mp, i, j);
	    break;
	}
    }

    /*
     * If we've already seen this sequence name, just free the
     * name string.  Else add it to the list of sequence names.
     */
    if (svector_at (mp->msgattrs, i)) {
	free (name);
    } else {
	svector_push_back (mp->msgattrs, name);
    }

    /*
     * Split up the different message ranges at whitespace
     */
    for (ap = brkstring (field, " ", "\n"); *ap; ap++) {
	if ((cp = strchr(*ap, '-')))
	    *cp++ = '\0';
	if ((j = m_atoi (*ap)) > 0) {
	    k = cp ? m_atoi (cp) : j;

	    /*
	     * Keep mp->curmsg and "cur" sequence in synch.  Unlike
	     * other sequences, this message doesn't need to exist.
	     * Think about the series of command (rmm; next) to
	     * understand why this can be the case.  But if it does
	     * exist, we will still set the bit flag for it like
	     * other sequences.
	     */
	    if (is_current)
		mp->curmsg = j;
	    /*
	     * We iterate through messages in this range
	     * and flip on bit for this sequence.
	     */
	    for (; j <= k; j++) {
		if (j >= mp->lowmsg && j <= mp->hghmsg && does_exist(mp, j))
		    add_sequence (mp, i, j);
	    }
	}
    }

    free (field);	/* free string containing message ranges */
    return i;
}
