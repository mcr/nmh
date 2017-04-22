/* m_draft.c -- construct the name of a draft message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>


char *
m_draft (char *folder, char *msg, int use, int *isdf)
{
    char *cp;
    struct msgs *mp;
    static char buffer[BUFSIZ];

    if (*isdf == -1 || folder == NULL || *folder == '\0') {
	if (*isdf == -1 || (cp = context_find ("Draft-Folder")) == NULL) {
	    *isdf = 0;
	    return m_maildir (msg && *msg ? msg : draft);
	}

        folder = path (*cp == '+' || *cp == '@' ? cp + 1 : cp,
                *cp != '@' ? TFOLDER : TSUBCWF);
    }
    *isdf = 1;
    
    if (chdir (m_maildir ("")) < 0) {
	advise (m_maildir (""), "chdir");
    }
    strncpy (buffer, m_maildir (folder), sizeof(buffer));

    create_folder (buffer, 0, done);

    if (chdir (buffer) == -1)
	adios (buffer, "unable to change directory to");

    if (!(mp = folder_read (folder, 1)))
	adios (NULL, "unable to read folder %s", folder);

    /*
     * Make sure we have enough message status space for all
     * the message numbers from 1 to "new", since we might
     * select an empty slot.  If we add more space at the
     * end, go ahead and add 10 additional slots.
     */
    if (mp->hghmsg >= mp->hghoff) {
	if (!(mp = folder_realloc (mp, 1, mp->hghmsg + 10)))
	    adios (NULL, "unable to allocate folder storage");
    } else if (mp->lowoff > 1) {
	if (!(mp = folder_realloc (mp, 1, mp->hghoff)))
	    adios (NULL, "unable to allocate folder storage");
    }

    mp->msgflags |= ALLOW_NEW;	/* allow the "new" sequence */

    /*
     * If we have been give a valid message name, then use that.
     * Else, if we are given the "use" option, then use the
     * current message.  Else, use special sequence "new".
     */
    if (!m_convert (mp, msg && *msg ? msg : use ? "cur" : "new"))
	done (1);
    seq_setprev (mp);

    if (mp->numsel > 1)
	adios (NULL, "only one message draft at a time!");

    snprintf (buffer, sizeof(buffer), "%s/%s", mp->foldpath, m_name (mp->lowsel));
    cp = buffer;

    seq_setcur (mp, mp->lowsel);/* set current message for folder */
    seq_save (mp);		/* synchronize message sequences  */
    folder_free (mp);		/* free folder/message structure  */

    return cp;
}
