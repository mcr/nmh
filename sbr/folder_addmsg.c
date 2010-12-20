
/*
 * folder_addmsg.c -- Link message into folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <errno.h>

/*
 * Link message into a folder.  Return the new number
 * of the message.  If an error occurs, return -1.
 */

int
folder_addmsg (struct msgs **mpp, char *msgfile, int selected,
               int unseen, int preserve, int deleting, char *from_dir)
{
    int infd, outfd, linkerr, msgnum;
    char *nmsg, newmsg[BUFSIZ];
    char oldmsg[BUFSIZ];
    struct msgs *mp;
    struct stat st1, st2;

    mp = *mpp;

    /* should we preserve the numbering of the message? */
    if (preserve && (msgnum = m_atoi (msgfile)) > 0) {
	;
    } else if (mp->nummsg == 0) {
	/* check if we are adding to empty folder */
	msgnum = 1;
    } else {
	/* else use highest message number + 1 */
	msgnum = mp->hghmsg + 1;
    }
     
    /*
     * We might need to make several attempts
     * in order to add the message to the folder.
     */
    for (;; msgnum++) {

	/*
	 * See if we need more space.  If we need space at the
	 * end, then we allocate space for an addition 100 messages.
	 * If we need space at the beginning of the range, then just
	 * extend message status range to cover this message number.
         */
	if (msgnum > mp->hghoff) {
	    if ((mp = folder_realloc (mp, mp->lowoff, msgnum + 100)))
		*mpp = mp;
	    else {
		advise (NULL, "unable to allocate folder storage");
		return -1;
	    }
	} else if (msgnum < mp->lowoff) {
	    if ((mp = folder_realloc (mp, msgnum, mp->hghoff)))
		*mpp = mp;
	    else {
		advise (NULL, "unable to allocate folder storage");
		return -1;
	    }
	}

	/*
	 * If a message is already in that slot,
	 * then loop to next available slot.
	 */
	if (does_exist (mp, msgnum))
	    continue;

	/* setup the bit flags for this message */
	clear_msg_flags (mp, msgnum);
	set_exists (mp, msgnum);

	/* should we set the SELECT_UNSEEN bit? */
	if (unseen) {
	    set_unseen (mp, msgnum);
	}

	/* should we set the SELECTED bit? */
	if (selected) {
	    set_selected (mp, msgnum);

	    /* check if highest or lowest selected */
	    if (mp->numsel == 0) {
		mp->lowsel = msgnum;
		mp->hghsel = msgnum;
	    } else {
		if (msgnum < mp->lowsel)
		    mp->lowsel = msgnum;
		if (msgnum > mp->hghsel)
		    mp->hghsel = msgnum;
	    }

	    /* increment number selected */
	    mp->numsel++;
	}

	/*
	 * check if this is highest or lowest message
         */
	if (mp->nummsg == 0) {
	    mp->lowmsg = msgnum;
	    mp->hghmsg = msgnum;
	} else {
	    if (msgnum < mp->lowmsg)
		mp->lowmsg = msgnum;
	    if (msgnum > mp->hghmsg)
		mp->hghmsg = msgnum;
	}

	/* increment message count */
	mp->nummsg++;

	nmsg = m_name (msgnum);
	snprintf (newmsg, sizeof(newmsg), "%s/%s", mp->foldpath, nmsg);

	/*
	 * Now try to link message into folder.
	 * Then run the external hook on the message if one was specified in the context.
	 * Run the refile hook if we're moving the message from one place to another.
	 * We have to construct the from path name for this because it's not there.
	 * Run the add hook if the message is getting copied or linked somewhere else.
	 */
	if (link (msgfile, newmsg) != -1) {

	    if (deleting) {
		(void)snprintf(oldmsg, sizeof (oldmsg), "%s/%s", from_dir, msgfile);
		(void)ext_hook("ref-hook", oldmsg, newmsg);
	    }
	    else
		(void)ext_hook("add-hook", newmsg, (char *)0);

	    return msgnum;
	} else {
	    linkerr = errno;

#ifdef EISREMOTE
	    if (linkerr == EISREMOTE)
		linkerr = EXDEV;
#endif /* EISREMOTE */

	    /*
	     * Check if the file in our desired location is the same
	     * as the source file.  If so, then just leave it alone
	     * and return.  Otherwise, we will continue the main loop
	     * and try again at another slot (hghmsg+1).
	     */
	    if (linkerr == EEXIST) {
		if (stat (msgfile, &st2) == 0 && stat (newmsg, &st1) == 0
		    && st2.st_ino == st1.st_ino) {
		    return msgnum;
		} else {
		    continue;
		}
	    }

	    /*
	     * If link failed because we are trying to link
	     * across devices, then check if there is a message
	     * already in the desired location.  If so, then return
	     * error, else just copy the message.
	     */
	    if (linkerr == EXDEV) {
		if (stat (newmsg, &st1) == 0) {
		    advise (NULL, "message %s:%s already exists", mp->foldpath, newmsg);
		    return -1;
		} else {
		    if ((infd = open (msgfile, O_RDONLY)) == -1) {
			advise (msgfile, "unable to open message %s", msgfile);
			return -1;
		    }
		    fstat (infd, &st1);
		    if ((outfd = creat (newmsg, (int) st1.st_mode & 0777)) == -1) {
			advise (newmsg, "unable to create");
			close (infd);
			return -1;
		    }
		    cpydata (infd, outfd, msgfile, newmsg);
		    close (infd);
		    close (outfd);

		    if (deleting) {
			(void)snprintf(oldmsg, sizeof (oldmsg), "%s/%s", from_dir, msgfile);
			(void)ext_hook("ref-hook", oldmsg, newmsg);
		    }
		    else
		        (void)ext_hook("add-hook", newmsg, (char *)0);

		    return msgnum;
		}
	    }

	    /*
	     * Else, some other type of link error,
	     * so just return error.
	     */
	    advise (newmsg, "error linking %s to", msgfile);
	    return -1;
	}
    }
}
