
/*
 * burst.c -- explode digests into individual messages
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

static struct swit switches[] = {
#define	INPLSW	0
    { "inplace", 0 },
#define	NINPLSW	1
    { "noinplace", 0 },
#define	QIETSW	2
    { "quiet", 0 },
#define	NQIETSW	3
    { "noquiet", 0 },
#define	VERBSW	4
    { "verbose", 0 },
#define	NVERBSW	5
    { "noverbose", 0 },
#define VERSIONSW 6
    { "version", 0 },
#define	HELPSW	7
    { "help", 0 },
    { NULL, 0 }
};

static char delim3[] = "-------";

struct smsg {
    long s_start;
    long s_stop;
};

/*
 * static prototypes
 */
static int find_delim (int, struct smsg *);
static void burst (struct msgs **, int, struct smsg *, int, int, int);
static void cpybrst (FILE *, FILE *, char *, char *, int);


int
main (int argc, char **argv)
{
    int inplace = 0, quietsw = 0, verbosw = 0;
    int msgp = 0, hi, msgnum, numburst;
    char *cp, *maildir, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments, *msgs[MAXARGS];
    struct smsg *smsgs;
    struct msgs *mp;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

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
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

	    case INPLSW: 
		inplace++;
		continue;
	    case NINPLSW: 
		inplace = 0;
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
	    else
		folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    msgs[msgp++] = cp;
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));
    if (!msgp)
	msgs[msgp++] = "cur";
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
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous-sequence */

    smsgs = (struct smsg *)
	calloc ((size_t) (MAXFOLDER + 2), sizeof(*smsgs));
    if (smsgs == NULL)
	adios (NULL, "unable to allocate burst storage");

    hi = mp->hghmsg + 1;

    /* burst all the SELECTED messages */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (is_selected (mp, msgnum)) {
	    if ((numburst = find_delim (msgnum, smsgs)) >= 1) {
		if (verbosw)
		    printf ("%d message%s exploded from digest %d\n",
			    numburst, numburst > 1 ? "s" : "", msgnum);
		burst (&mp, msgnum, smsgs, numburst, inplace, verbosw);
	    } else {
		if (numburst == 0) {
		    if (!quietsw)
			admonish (NULL, "message %d not in digest format",
				  msgnum);
		}  /* this pair of braces was missing before 1999-07-15 */
		else
		    adios (NULL, "burst() botch -- you lose big");
	    }
	}
    }

    free ((char *) smsgs);
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
    return done (0);
}


/*
 * Scan the message and find the beginning and
 * end of all the messages in the digest.
 */

static int
find_delim (int msgnum, struct smsg *smsgs)
{
    int ld3, wasdlm, msgp;
    long pos;
    char c, *msgnam;
    int cc;
    char buffer[BUFSIZ];
    FILE *in;

    ld3 = strlen (delim3);

    if ((in = fopen (msgnam = m_name (msgnum), "r")) == NULL)
	adios (msgnam, "unable to read message");

    for (msgp = 0, pos = 0L; msgp <= MAXFOLDER;) {
	while (fgets (buffer, sizeof(buffer), in) && buffer[0] == '\n')
	    pos += (long) strlen (buffer);
	if (feof (in))
	    break;
	fseek (in, pos, SEEK_SET);
	smsgs[msgp].s_start = pos;

	for (c = 0; fgets (buffer, sizeof(buffer), in); c = buffer[0]) {
	    if (strncmp (buffer, delim3, ld3) == 0
		    && (msgp == 1 || c == '\n')
		    && ((cc = peekc (in)) == '\n' || cc == EOF))
		break;
	    else
		pos += (long) strlen (buffer);
	}

	wasdlm = strncmp (buffer, delim3, ld3) == 0;
	if (smsgs[msgp].s_start != pos)
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
    return (msgp - 1);		/* toss "End of XXX Digest" */
}


/*
 * Burst out the messages in the digest into the folder
 */

static void
burst (struct msgs **mpp, int msgnum, struct smsg *smsgs, int numburst,
	int inplace, int verbosw)
{
    int i, j, mode;
    char *msgnam;
    char f1[BUFSIZ], f2[BUFSIZ], f3[BUFSIZ];
    FILE *in, *out;
    struct stat st;
    struct msgs *mp;

    if ((in = fopen (msgnam = m_name (msgnum), "r")) == NULL)
	adios (msgnam, "unable to read message");

    mode = fstat (fileno(in), &st) != NOTOK ? (st.st_mode & 0777) : m_gmprot();
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
		copy_msg_flags (mp, i, j);
		clear_msg_flags (mp, j);
		mp->msgflags |= SEQMOD;
	    }
	}
    }
    
    unset_selected (mp, msgnum);

    /* new hghmsg is hghmsg + numburst */
    i = inplace ? msgnum + numburst : mp->hghmsg;
    for (j = numburst; j >= (inplace ? 0 : 1); i--, j--) {
	strncpy (f1, m_name (i), sizeof(f1));
	strncpy (f2, m_scratch ("", invo_name), sizeof(f2));
	if (verbosw && i != msgnum)
	    printf ("message %d of digest %d becomes message %d\n", j, msgnum, i);

	if ((out = fopen (f2, "w")) == NULL)
	    adios (f2, "unable to write message");
	chmod (f2, mode);
	fseek (in, smsgs[j].s_start, SEEK_SET);
	cpybrst (in, out, msgnam, f2,
		(int) (smsgs[j].s_stop - smsgs[j].s_start));
	fclose (out);

	if (i == msgnum) {
	    strncpy (f3, m_backup (f1), sizeof(f3));
	    if (rename (f1, f3) == NOTOK)
		admonish (f3, "unable to rename %s to", f1);
	}
	if (rename (f2, f1) == NOTOK)
	    admonish (f1, "unable to rename %s to", f2);
	copy_msg_flags (mp, i, msgnum);
	mp->msgflags |= SEQMOD;
    }

    fclose (in);
}


#define	S1  0
#define	S2  1
#define	S3  2

/*
 * Copy a mesage which is being burst out of a digest.
 * It will remove any "dashstuffing" in the message.
 */

static void
cpybrst (FILE *in, FILE *out, char *ifile, char *ofile, int len)
{
    register int c, state;

    for (state = S1; (c = fgetc (in)) != EOF && len > 0; len--) {
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
		    case '\n': 
			fputc (c, out);
			break;
		}
		break;

	    case S2: 
		switch (c) {
		    case '\n': 
			state = S1;
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
	}
    }

    if (ferror (in) && !feof (in))
	adios (ifile, "error reading");
    if (ferror (out))
	adios (ofile, "error writing");
}
