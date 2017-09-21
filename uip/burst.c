/* burst.c -- explode digests into individual messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/mhparse.h>
#include "h/done.h"
#include "sbr/m_maildir.h"
#include "sbr/m_mktemp.h"
#include "mhfree.h"

#define BURST_SWITCHES \
    X("inplace", 0, INPLSW) \
    X("noinplace", 0, NINPLSW) \
    X("mime", 0, MIMESW) \
    X("nomime", 0, NMIMESW) \
    X("automime", 0, AUTOMIMESW) \
    X("quiet", 0, QIETSW) \
    X("noquiet", 0, NQIETSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(BURST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(BURST, switches);
#undef X

struct smsg {
    off_t s_start;
    off_t s_stop;
};

/*
 * For the MIME parsing routines
 */

int debugsw = 0;

/*
 * static prototypes
 */
static int find_delim (int, struct smsg *, int *);
static void find_mime_parts (CT, struct smsg *, int *);
static void burst (struct msgs **, int, struct smsg *, int, int, int,
		   char *, int);
static void cpybrst (FILE *, FILE *, char *, char *, int, int);

/*
 * A macro to check to see if we have reached a message delimiter
 * (an encapsulation boundary, EB, in RFC 934 parlance).
 *
 * According to RFC 934, an EB is simply a line which starts with
 * a "-" and is NOT followed by a space.  So even a single "-" on a line
 * by itself would be an EB.
 */

#define CHECKDELIM(buffer) (buffer[0] == '-' && buffer[1] != ' ')

int
main (int argc, char **argv)
{
    int inplace = 0, quietsw = 0, verbosw = 0, mimesw = 1;
    int hi, msgnum, numburst;
    char *cp, *maildir, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct smsg *smsgs;
    struct msgs *mp;

    if (nmh_init(argv[0], 1)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW: 
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW: 
		adios (NULL, "-%s unknown\n", cp);

	    case HELPSW: 
		snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case INPLSW: 
		inplace++;
		continue;
	    case NINPLSW: 
		inplace = 0;
		continue;

	    case MIMESW:
	    	mimesw = 2;
		continue;
	    case NMIMESW:
	    	mimesw = 0;
		continue;
	    case AUTOMIMESW:
	    	mimesw = 1;
		continue;

	    case QIETSW: 
		quietsw++;
		continue;
	    case NQIETSW: 
		quietsw = 0;
		continue;

	    case VERBSW: 
		verbosw++;
		continue;
	    case NVERBSW: 
		verbosw = 0;
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
            folder = pluspath (cp);
	} else {
	    app_msgarg(&msgs, cp);
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!msgs.size)
	app_msgarg(&msgs, "cur");
    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder, 1)))
	adios (NULL, "unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	adios (NULL, "no messages in %s", folder);

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < msgs.size; msgnum++)
	if (!m_convert (mp, msgs.msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous-sequence */

    smsgs = mh_xcalloc(MAXFOLDER + 2, sizeof *smsgs);

    hi = mp->hghmsg + 1;

    /* burst all the SELECTED messages */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum)) {
	    if ((numburst = find_delim (msgnum, smsgs, &mimesw)) >= 1) {
		if (verbosw)
		    printf ("%d message%s exploded from digest %d\n",
			    numburst, PLURALS(numburst), msgnum);
		burst (&mp, msgnum, smsgs, numburst, inplace, verbosw,
		       maildir, mimesw);
	    } else {
		if (numburst == 0) {
		    if (!quietsw)
			inform("message %d not in digest format, continuing...",
				  msgnum);
		}  /* this pair of braces was missing before 1999-07-15 */
		else
		    adios (NULL, "burst() botch -- you lose big");
	    }
	}
    }

    free(smsgs);
    context_replace (pfolder, folder);	/* update current folder */

    /*
     * If -inplace is given, then the first message burst becomes
     * the current message (which will now show a table of contents).
     * Otherwise, the first message extracted from the first digest
     * becomes the current message.
     */
    if (inplace) {
	if (mp->lowsel != mp->curmsg)
	    seq_setcur (mp, mp->lowsel);
    } else {
	if (hi <= mp->hghmsg)
	    seq_setcur (mp, hi);
    }

    seq_save (mp);	/* synchronize message sequences */
    context_save ();	/* save the context file         */
    folder_free (mp);	/* free folder/message structure */
    done (0);
    return 1;
}


/*
 * Scan the message and find the beginning and
 * end of all the messages in the digest.
 *
 * If requested, see if the message is MIME-formatted and contains any
 * message/rfc822 parts; if so, burst those parts.
 */

static int
find_delim (int msgnum, struct smsg *smsgs, int *mimesw)
{
    int wasdlm = 0, msgp;
    off_t pos;
    char c, *msgnam;
    char buffer[BUFSIZ];
    FILE *in;
    CT content;

    msgnam = m_name (msgnum);

    /*
     * If mimesw is 1 or 2, try to see if it's got proper MIME formatting.
     */

    if (*mimesw > 0) {
    	content = parse_mime(msgnam);
	if (! content && *mimesw == 2)
	    return 0;
	if (content) {
	    smsgs[0].s_start = 0;
	    smsgs[0].s_stop = content->c_begin - 1;
	    msgp = 1;
	    find_mime_parts(content, smsgs, &msgp);
	    free_content(content);
	    if (msgp == 1 && *mimesw == 2)
	    	adios (msgnam, "does not have any message/rfc822 parts");
	    if (msgp > 1) {
	    	*mimesw = 1;
		return msgp - 1;
	    }
	}
    }

    *mimesw = 0;

    if ((in = fopen (msgnam, "r")) == NULL)
	adios (msgnam, "unable to read message");

    for (msgp = 0, pos = 0L; msgp <= MAXFOLDER;) {
    	/*
	 * We're either at the beginning of the whole message, or
	 * we're just past the delimiter of the last message.
	 * Swallow lines until we get to something that's not a newline
	 */
	while (fgets (buffer, sizeof(buffer), in) && buffer[0] == '\n')
	    pos += (long) strlen (buffer);
	if (feof (in))
	    break;

	/*
	 * Reset to the beginning of the last non-blank line, and save our
	 * starting position.  This is where the encapsulated message
	 * starts.
	 */
	fseeko (in, pos, SEEK_SET);
	smsgs[msgp].s_start = pos;

	/*
	 * Read in lines until we get to a message delimiter.
	 *
	 * Previously we checked to make sure the preceding line and
	 * next line was a newline.  That actually does not comply with
	 * RFC 934, so make sure we break on a message delimiter even
	 * if the previous character was NOT a newline.
	 */
	for (c = 0; fgets (buffer, sizeof(buffer), in); c = buffer[0]) {
	    if ((wasdlm = CHECKDELIM(buffer)))
		break;
            pos += (long) strlen (buffer);
	}

	/*
	 * Only count as a new message if we got the message delimiter.
	 * Swallow a blank line if it was right before the message delimiter.
	 */
	if (smsgs[msgp].s_start != pos && wasdlm)
	    smsgs[msgp++].s_stop = (c == '\n' && wasdlm) ? pos - 1 : pos;

	if (feof (in)) {
#if 0
	    if (wasdlm) {
		smsgs[msgp - 1].s_stop -= ((long) strlen (buffer) + 1);
		msgp++;		/* fake "End of XXX Digest" */
	    }
#endif
	    break;
	}
	pos += (long) strlen (buffer);
    }

    fclose (in);
    return msgp - 1;		/* return the number of messages burst */
}


/*
 * Find any MIME content in the message that is a message/rfc822 and add
 * it to the list of messages to burst.
 */

static void
find_mime_parts (CT content, struct smsg *smsgs, int *msgp)
{
    struct multipart *m;
    struct part *part;

    /*
     * If we have a message/rfc822, then it's easy.
     */

    if (content->c_type == CT_MESSAGE &&
    			content->c_subtype == MESSAGE_RFC822) {
	smsgs[*msgp].s_start = content->c_begin;
	smsgs[*msgp].s_stop = content->c_end;
	(*msgp)++;
	return;
    }

    /*
     * Otherwise, if we do have multiparts, try all of the sub-parts.
     */

    if (content->c_type == CT_MULTIPART) {
    	m = (struct multipart *) content->c_ctparams;

	for (part = m->mp_parts; part; part = part->mp_next)
	    find_mime_parts(part->mp_part, smsgs, msgp);
    }
}


/*
 * Burst out the messages in the digest into the folder
 */

static void
burst (struct msgs **mpp, int msgnum, struct smsg *smsgs, int numburst,
	int inplace, int verbosw, char *maildir, int mimesw)
{
    int i, j, mode;
    char *msgnam;
    char f1[BUFSIZ], f2[BUFSIZ], f3[BUFSIZ];
    FILE *in, *out;
    struct stat st;
    struct msgs *mp;

    if ((in = fopen (msgnam = m_name (msgnum), "r")) == NULL)
	adios (msgnam, "unable to read message");

    mode =
      fstat (fileno(in), &st) != NOTOK ? (int) (st.st_mode & 0777) : m_gmprot();
    mp = *mpp;

    /*
     * See if we have enough space in the folder
     * structure for all the new messages.
     */
    if ((mp->hghmsg + numburst > mp->hghoff) &&
	!(mp = folder_realloc (mp, mp->lowoff, mp->hghmsg + numburst)))
	adios (NULL, "unable to allocate folder storage");
    *mpp = mp;

    j = mp->hghmsg;		/* old value */
    mp->hghmsg += numburst;
    mp->nummsg += numburst;

    /*
     * If this is not the highest SELECTED message, then
     * increment mp->hghsel by numburst, since the highest
     * SELECTED is about to be slid down by that amount.
     */
    if (msgnum < mp->hghsel)
	mp->hghsel += numburst;

    /*
     * If -inplace is given, renumber the messages after the
     * source message, to make room for each of the messages
     * contained within the digest.
     *
     * This is equivalent to refiling a message from the point
     * of view of the external hooks.
     */
    if (inplace) {
	for (i = mp->hghmsg; j > msgnum; i--, j--) {
	    strncpy (f1, m_name (i), sizeof(f1));
	    strncpy (f2, m_name (j), sizeof(f2));
	    if (does_exist (mp, j)) {
		if (verbosw)
		    printf ("message %d becomes message %d\n", j, i);

		if (rename (f2, f1) == NOTOK)
		    admonish (f1, "unable to rename %s to", f2);

		(void)snprintf(f1, sizeof (f1), "%s/%d", maildir, i);
		(void)snprintf(f2, sizeof (f2), "%s/%d", maildir, j);
		ext_hook("ref-hook", f1, f2);

		copy_msg_flags (mp, i, j);
		clear_msg_flags (mp, j);
		mp->msgflags |= SEQMOD;
	    }
	}
    }
    
    unset_selected (mp, msgnum);

    /* new hghmsg is hghmsg + numburst
     *
     * At this point, there is an array of numburst smsgs, each element of
     * which contains the starting and stopping offsets (seeks) of the message
     * in the digest.  The inplace flag is set if the original digest is replaced
     * by a message containing the table of contents.  smsgs[0] is that table of
     * contents.  Go through the message numbers in reverse order (high to low).
     *
     * Set f1 to the name of the destination message, f2 to the name of a scratch
     * file.  Extract a message from the digest to the scratch file.  Move the
     * original message to a backup file if the destination message number is the
     * same as the number of the original message, which only happens if the
     * inplace flag is set.  Then move the scratch file to the destination message.
     *
     * Moving the original message to the backup file is equivalent to deleting the
     * message from the point of view of the external hooks.  And bursting each
     * message is equivalent to adding a new message.
     */

    i = inplace ? msgnum + numburst : mp->hghmsg;
    for (j = numburst; j >= (inplace ? 0 : 1); i--, j--) {
        char *tempfile;

	if ((tempfile = m_mktemp2(NULL, invo_name, NULL, &out)) == NULL) {
	    adios(NULL, "unable to create temporary file in %s",
		  get_temp_dir());
	}
	strncpy (f2, tempfile, sizeof(f2));
	strncpy (f1, m_name (i), sizeof(f1));

	if (verbosw && i != msgnum)
	    printf ("message %d of digest %d becomes message %d\n", j, msgnum, i);

	chmod (f2, mode);
	fseeko (in, smsgs[j].s_start, SEEK_SET);
	cpybrst (in, out, msgnam, f2,
		(int) (smsgs[j].s_stop - smsgs[j].s_start), mimesw);
	fclose (out);

	if (i == msgnum) {
	    strncpy (f3, m_backup (f1), sizeof(f3));
	    if (rename (f1, f3) == NOTOK)
		admonish (f3, "unable to rename %s to", f1);

	    (void)snprintf(f3, sizeof (f3), "%s/%d", maildir, i);
	    ext_hook("del-hook", f3, NULL);
	}
	if (rename (f2, f1) == NOTOK)
	    admonish (f1, "unable to rename %s to", f2);

	(void)snprintf(f3, sizeof (f3), "%s/%d", maildir, i);
	ext_hook("add-hook", f3, NULL);

	copy_msg_flags (mp, i, msgnum);
	mp->msgflags |= SEQMOD;
    }

    fclose (in);
}


#define	S1  0
#define	S2  1
#define	S3  2
#define S4  3

/*
 * Copy a message which is being burst out of a digest.
 * It will remove any "dashstuffing" in the message.
 */

static void
cpybrst (FILE *in, FILE *out, char *ifile, char *ofile, int len, int mime)
{
    int c, state;

    for (state = mime ? S4 : S1; (c = fgetc (in)) != EOF && len > 0; len--) {
	if (c == 0)
	    continue;
	switch (state) {
	    case S1: 
		switch (c) {
		    case '-': 
			state = S3;
			break;

		    default: 
			state = S2;
			/* FALLTHRU */
		    case '\n': 
			fputc (c, out);
			break;
		}
		break;

	    case S2: 
		switch (c) {
		    case '\n': 
			state = S1;
			/* FALLTHRU */
		    default: 
			fputc (c, out);
			break;
		}
		break;

	    case S3: 
		switch (c) {
		    case ' ': 
			state = S2;
			break;

		    default: 
			state = (c == '\n') ? S1 : S2;
			fputc ('-', out);
			fputc (c, out);
			break;
		}
		break;

	    case S4:
	   	fputc (c, out);
		break;
	}
    }

    if (ferror (in) && !feof (in))
	adios (ifile, "error reading");
    if (ferror (out))
	adios (ofile, "error writing");
}
