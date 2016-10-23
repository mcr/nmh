
/*
 * folder_read.c -- initialize folder structure and read folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

/* We allocate the `mi' array 1024 elements at a time */
#define	NUMMSGS  1024

/*
 * 1) Create the folder/message structure
 * 2) Read the directory (folder) and temporarily
 *    record the numbers of the messages we have seen.
 * 3) Then allocate the array for message attributes and
 *    set the initial flags for all messages we've seen.
 * 4) Read and initialize the sequence information.
 */

struct msgs *
folder_read (char *name, int lockflag)
{
    int msgnum, len, *mi;
    struct msgs *mp;
    struct dirent *dp;
    DIR *dd;
    bvector_t *v;
    size_t i;

    name = m_mailpath (name);
    if (!(dd = opendir (name))) {
	free (name);
	return NULL;
    }

    /* Allocate the main structure for folder information */
    NEW(mp);
    clear_folder_flags (mp);
    mp->foldpath = name;
    mp->lowmsg = 0;
    mp->hghmsg = 0;
    mp->curmsg = 0;
    mp->lowsel = 0;
    mp->hghsel = 0;
    mp->numsel = 0;
    mp->nummsg = 0;
    mp->seqhandle = NULL;
    mp->seqname = NULL;

    if (access (name, W_OK) == -1)
	set_readonly (mp);

    /*
     * Allocate a temporary place to record the
     * name of the messages in this folder.
     */
    len = NUMMSGS;
    mi = (int *) mh_xmalloc ((size_t) (len * sizeof(*mi)));

    while ((dp = readdir (dd))) {
	if ((msgnum = m_atoi (dp->d_name)) && msgnum > 0) {
	    /*
	     * Check if we need to allocate more
	     * temporary elements for message names.
	     */
	    if (mp->nummsg >= len) {
		len += NUMMSGS;
		mi = (int *) mh_xrealloc (mi, (size_t) (len * sizeof(*mi)));
	    }

	    /* Check if this is the first message we've seen */
	    if (mp->nummsg == 0) {
		mp->lowmsg = msgnum;
		mp->hghmsg = msgnum;
	    } else {
		/* Check if this is it the highest or lowest we've seen? */
		if (msgnum < mp->lowmsg)
		   mp->lowmsg = msgnum;
		if (msgnum > mp->hghmsg)
		   mp->hghmsg = msgnum;
	    }

	    /*
	     * Now increment count, and record message
	     * number in a temporary place for now.
	     */
	    mi[mp->nummsg++] = msgnum;

	} else {
	    switch (dp->d_name[0]) {
		case '.': 
		case ',': 
		    continue;

		default: 
		    /* skip any files beginning with backup prefix */
		    if (HasPrefix(dp->d_name, BACKUP_PREFIX))
			continue;

		    /* skip the LINK file */
		    if (!strcmp (dp->d_name, LINK))
			continue;

		    /* indicate that there are other files in folder */
		    set_other_files (mp);
		    continue;
	    }
	}
    }

    closedir (dd);
    mp->lowoff = max (mp->lowmsg, 1);

    /* Go ahead and allocate space for 100 additional messages. */
    mp->hghoff = mp->hghmsg + 100;

    /* for testing, allocate minimal necessary space */
    /* mp->hghoff = max (mp->hghmsg, 1); */

    /*
     * If for some reason hghoff < lowoff (like we got an integer overflow)
     * the complain about this now.
     */

    if (mp->hghoff < mp->lowoff) {
	adios(NULL, "Internal failure: high message limit < low message "
	      "limit; possible overflow?");
    }

    /*
     * Allocate space for status of each message.
     */
    mp->num_msgstats = MSGSTATNUM (mp->lowoff, mp->hghoff);
    mp->msgstats = mh_xmalloc (MSGSTATSIZE(mp));
    for (i = 0, v = mp->msgstats; i < mp->num_msgstats; ++i, ++v) {
	*v = bvector_create (0);
    }

    mp->msgattrs = svector_create (0);

    /*
     * Clear all the flag bits for all the message
     * status entries we just allocated.
     */
    for (msgnum = mp->lowoff; msgnum <= mp->hghoff; msgnum++)
	clear_msg_flags (mp, msgnum);

    /*
     * Scan through the array of messages we've seen and
     * setup the initial flags for those messages in the
     * newly allocated mp->msgstats area.
     */
    for (msgnum = 0; msgnum < mp->nummsg; msgnum++)
	set_exists (mp, mi[msgnum]);

    free (mi);		/* We don't need this anymore    */

    /*
     * Read and initialize the sequence information.
     */
    if (seq_read (mp, lockflag) == NOTOK) {
        char seqfile[PATH_MAX];

        /* Failed to lock sequence file. */
        snprintf (seqfile, sizeof(seqfile), "%s/%s", mp->foldpath, mh_seq);
        advise (seqfile, "failed to lock");

        return NULL;
    }

    return mp;
}
