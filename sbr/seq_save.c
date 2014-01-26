
/*
 * seq_save.c -- 1) synchronize sequences
 *            -- 2) save public sequences
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>


/*
 * 1.  If sequence is public and folder is readonly,
 *     then change it to be private
 * 2a. If sequence is public, then add it to the sequences file
 *     in folder (name specified by mh-sequences profile entry).
 * 2b. If sequence is private, then add it to the
 *     context file.
 */

void
seq_save (struct msgs *mp)
{
    size_t i;
    char flags, *cp, attr[BUFSIZ], seqfile[PATH_MAX];
    FILE *fp;
    sigset_t set, oset;

    /* check if sequence information has changed */
    if (!(mp->msgflags & SEQMOD)) {
    	if (mp->seqhandle) {
	    lkfclosedata (mp->seqhandle, mp->seqname);
	    mp->seqhandle = NULL;
	    free(mp->seqname);
	    mp->seqname = NULL;
	}
	return;
    }
    mp->msgflags &= ~SEQMOD;

    fp = NULL;
    flags = mp->msgflags;	/* record folder flags */

    /*
     * If no mh-sequences file is defined, or if a mh-sequences file
     * is defined but empty (*mh_seq == '\0'), then pretend folder
     * is readonly.  This will force all sequences to be private.
     */
    if (mh_seq == NULL || *mh_seq == '\0')
	set_readonly (mp);
    else
	snprintf (seqfile, sizeof(seqfile), "%s/%s", mp->foldpath, mh_seq);

    for (i = 0; i < svector_size (mp->msgattrs); i++) {
	snprintf (attr, sizeof(attr), "atr-%s-%s",
		  svector_at (mp->msgattrs, i), mp->foldpath);

	/* get space separated list of sequence ranges */
	if (!(cp = seq_list(mp, svector_at (mp->msgattrs, i)))) {
	    context_del (attr);			/* delete sequence from context */
	    continue;
	}

	if (is_readonly(mp) || is_seq_private(mp, i)) {
priv:
	    /*
	     * sequence is private
	     */
	    context_replace (attr, cp);		/* update sequence in context   */
	} else {
	    /*
	     * sequence is public
	     */
	    context_del (attr);			/* delete sequence from context */

	    if (!fp) {
		/*
		 * Attempt to open file for public sequences.
		 * If that fails (probably because folder is
		 * readonly), then make sequence private.
		 */

		if (mp->seqhandle) {
		    fp = mp->seqhandle;
		    mp->seqhandle = NULL;
		    free(mp->seqname);
		    mp->seqname = NULL;
		    rewind(fp);
		    ftruncate(fileno(fp), 0);
		} else if ((fp = lkfopendata (seqfile, "w")) == NULL
			&& (m_unlink (seqfile) == -1 ||
			    (fp = lkfopendata (seqfile, "w")) == NULL)) {
		    admonish (attr, "unable to write");
		    goto priv;
		}

		/* block a few signals */
		sigemptyset (&set);
		sigaddset(&set, SIGHUP);
		sigaddset(&set, SIGINT);
		sigaddset(&set, SIGQUIT);
		sigaddset(&set, SIGTERM);
		sigprocmask (SIG_BLOCK, &set, &oset);
	    }
	    fprintf (fp, "%s: %s\n", svector_at (mp->msgattrs, i), cp);
	}
    }

    if (fp) {
	lkfclosedata (fp, seqfile);
	sigprocmask (SIG_SETMASK, &oset, &set);  /* reset signal mask */
    } else {
	/*
	 * If folder is not readonly, and we didn't save any
	 * public sequences, then remove that file.
	 */
	if (!is_readonly(mp))
	    (void) m_unlink (seqfile);
    }

    /*
     * Reset folder flag, since we may be
     * pretending that folder is readonly.
     */
    mp->msgflags = flags;
}
