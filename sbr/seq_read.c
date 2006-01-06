
/*
 * seq_read.c -- read the .mh_sequence file and
 *            -- initialize sequence information
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

/*
 * static prototypes
 */
static int seq_init (struct msgs *, char *, char *);
static void seq_public (struct msgs *);
static void seq_private (struct msgs *);


/*
 * Get the sequence information for this folder from
 * .mh_sequences (or equivalent specified in .mh_profile)
 * or context file (for private sequences).
 */

void
seq_read (struct msgs *mp)
{
    /*
     * Initialize the list of sequence names.  Go ahead and
     * add the "cur" sequence to the list of sequences.
     */
    mp->msgattrs[0] = getcpy (current);
    mp->msgattrs[1] = NULL;
    make_all_public (mp);	/* initially, make all public */

    /* If folder is empty, don't scan for sequence information */
    if (mp->nummsg == 0)
	return;

    /* Initialize the public sequences */
    seq_public (mp);

    /* Initialize the private sequences */
    seq_private (mp);
}


/*
 * read folder's sequences file for public sequences
 */

static void
seq_public (struct msgs *mp)
{
    int state;
    char *cp, seqfile[PATH_MAX];
    char name[NAMESZ], field[BUFSIZ];
    FILE *fp;

    /*
     * If mh_seq == NULL (such as if nmh been compiled with
     * NOPUBLICSEQ), or if *mh_seq == '\0' (the user has defined
     * the "mh-sequences" profile entry, but left it empty),
     * then just return, and do not initialize any public sequences.
     */
    if (mh_seq == NULL || *mh_seq == '\0')
	return;

    /* get filename of sequence file */
    snprintf (seqfile, sizeof(seqfile), "%s/%s", mp->foldpath, mh_seq);

    if ((fp = lkfopen (seqfile, "r")) == NULL)
	return;

    /* Use m_getfld to scan sequence file */
    for (state = FLD;;) {
	switch (state = m_getfld (state, name, field, sizeof(field), fp)) {
	    case FLD: 
	    case FLDPLUS:
	    case FLDEOF: 
		if (state == FLDPLUS) {
		    cp = getcpy (field);
		    while (state == FLDPLUS) {
			state = m_getfld (state, name, field, sizeof(field), fp);
			cp = add (field, cp);
		    }
		    seq_init (mp, getcpy (name), trimcpy (cp));
		    free (cp);
		} else {
		    seq_init (mp, getcpy (name), trimcpy (field));
		}
		if (state == FLDEOF)
		    break;
		continue;

	    case BODY: 
	    case BODYEOF: 
		adios (NULL, "no blank lines are permitted in %s", seqfile);
		/* fall */

	    case FILEEOF:
		break;

	    default: 
		adios (NULL, "%s is poorly formatted", seqfile);
	}
	break;	/* break from for loop */
    }

    lkfclose (fp, seqfile);
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

    alen = strlen ("atr-");
    plen = strlen (mp->foldpath) + 1;

    for (np = m_defs; np; np = np->n_next) {
	if (ssequal ("atr-", np->n_name)
		&& (j = strlen (np->n_name) - plen) > alen
		&& *(np->n_name + j) == '-'
		&& strcmp (mp->foldpath, np->n_name + j + 1) == 0) {
	    cp = getcpy (np->n_name + alen);
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
    int i, j, k, is_current;
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
     * mesages in this folder.
     */
    for (i = 0; mp->msgattrs[i]; i++) {
	if (!strcmp (mp->msgattrs[i], name)) {
	    for (j = mp->lowmsg; j <= mp->hghmsg; j++)
		clear_sequence (mp, i, j);
	    break;
	}
    }

    /* Return error, if too many sequences */
    if (i >= NUMATTRS) {
	free (name);
	free (field);
	return -1;
    }

    /*
     * If we've already seen this sequence name, just free the
     * name string.  Else add it to the list of sequence names.
     */
    if (mp->msgattrs[i]) {
	free (name);
    } else {
	mp->msgattrs[i] = name;
	mp->msgattrs[i + 1] = NULL;
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
