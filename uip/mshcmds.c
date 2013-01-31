
/*
 * mshcmds.c -- command handlers in msh
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include <h/dropsbr.h>
#include <h/fmt_scan.h>
#include <h/scansbr.h>
#include <h/tws.h>
#include <h/mts.h>
#include <errno.h>
#include <signal.h>
#include <h/msh.h>
#include <h/picksbr.h>
#include <h/utils.h>


static char delim3[] = "-------";	/* from burst.c */

static int mhlnum;
static FILE *mhlfp;

/*
 * Type for a compare function for qsort.  This keeps
 * the compiler happy.
 */
typedef int (*qsort_comp) (const void *, const void *);

/*
 * static prototypes
 */
static int burst (struct Msg *, int, int, int, int);
static void forw (char *, char *, int, char **);
static void rmm (void);
static void show (int);
static int eom_action (int);
static FILE *mhl_action (char *);
static int ask (int);
static int is_nontext (int);
static int get_fields (char *, char *, int, struct Msg *);
static int msgsort (struct Msg *, struct Msg *);
static int subsort (struct Msg *, struct Msg *);
static char *sosmash (char *, char *);
static int process (int, char *, int, char **);
static void copy_message (int, FILE *);
static void copy_digest (int, FILE *);

extern m_getfld_state_t gstate;	/* use the gstate in scansbr.c */

void
forkcmd (char **args, char *pgm)
{
    int child_id;
    char *vec[MAXARGS];

    vec[0] = r1bindex (pgm, '/');
    copyip (args, vec + 1, MAXARGS - 1);

    if (fmsh) {
	context_del (pfolder);
	context_replace (pfolder, fmsh);/* update current folder   */
	seq_save (mp);
	context_save ();		/* save the context file   */
    }
    fflush (stdout);
    switch (child_id = fork ()) {
	case NOTOK: 
	    advise ("fork", "unable to");
	    return;

	case OK: 
	    closefds (3);
	    SIGNAL (SIGINT, istat);
	    SIGNAL (SIGQUIT, qstat);

	    execvp (pgm, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (cmd_name);
	    _exit (1);

	default: 
	    pidXwait (child_id, NULL);
	    break;
    }
    if (fmsh) {			/* assume the worst case */
	mp->msgflags |= MODIFIED;
	modified++;
    }
}


#define DIST_SWITCHES \
    X("annotate", 0, DIANSW) \
    X("noannotate", 0, DINANSW) \
    X("draftfolder +folder", 0, DIDFSW) \
    X("draftmessage msg", 0, DIDMSW) \
    X("nodraftfolder", 0, DINDFSW) \
    X("editor editor", 0, DIEDTSW) \
    X("noedit", 0, DINEDSW) \
    X("form formfile", 0, DIFRMSW) \
    X("inplace", 0, DIINSW) \
    X("noinplace", 0, DININSW) \
    X("whatnowproc program", 0, DIWHTSW) \
    X("nowhatnowproc", 0, DINWTSW) \
    X("help", 0, DIHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(DIST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(DIST, distswit);
#undef X


void
distcmd (char **args)
{
    int vecp = 1;
    char *cp, *msg = NULL;
    char buf[BUFSIZ], *vec[MAXARGS];

    if (fmsh) {
	forkcmd (args, cmd_name);
	return;
    }

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, distswit)) {
		case AMBIGSW: 
		    ambigsw (cp, distswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case DIHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, distswit, 1);
		    return;

		case DIANSW:	/* not implemented */
		case DINANSW: 
		case DIINSW: 
		case DININSW: 
		    continue;

		case DINDFSW:
		case DINEDSW:
		case DINWTSW:
		    vec[vecp++] = --cp;
		    continue;

		case DIEDTSW: 
		case DIFRMSW: 
		case DIDFSW:
		case DIDMSW:
		case DIWHTSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    vec[vecp++] = cp;
		    continue;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    if (msg) {
		advise (NULL, "only one message at a time!");
		return;
	    }
	    else
		msg = cp;
    }

    vec[0] = cmd_name;
    vec[vecp++] = "-file";
    vec[vecp] = NULL;
    if (!msg)
	msg = "cur";
    if (!m_convert (mp, msg))
	return;
    seq_setprev (mp);

    if (mp->numsel > 1) {
	advise (NULL, "only one message at a time!");
	return;
    }
    process (mp->hghsel, cmd_name, vecp, vec);
    seq_setcur (mp, mp->hghsel);
}


#define EXPLODE_SWITCHES \
    X("inplace", 0, EXINSW) \
    X("noinplace", 0, EXNINSW) \
    X("quiet", 0, EXQISW) \
    X("noquiet", 0, EXNQISW) \
    X("verbose", 0, EXVBSW) \
    X("noverbose", 0, EXNVBSW) \
    X("help", 0, EXHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(EXPLODE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(EXPLODE, explswit);
#undef X


void
explcmd (char **args)
{
    int inplace = 0, quietsw = 0, verbosw = 0;
    int msgp = 0, hi, msgnum;
    char *cp, buf[BUFSIZ], *msgs[MAXARGS];
    struct Msg *smsgs;

    if (fmsh) {
	forkcmd (args, cmd_name);
	return;
    }

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, explswit)) {
		case AMBIGSW: 
		    ambigsw (cp, explswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case EXHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, explswit, 1);
		    return;

		case EXINSW: 
		    inplace++;
		    continue;
		case EXNINSW: 
		    inplace = 0;
		    continue;
		case EXQISW: 
		    quietsw++;
		    continue;
		case EXNQISW: 
		    quietsw = 0;
		    continue;
		case EXVBSW: 
		    verbosw++;
		    continue;
		case EXNVBSW: 
		    verbosw = 0;
		    continue;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    msgs[msgp++] = cp;
    }

    if (!msgp)
	msgs[msgp++] = "cur";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    smsgs = (struct Msg *)
		calloc ((size_t) (MAXFOLDER + 2), sizeof *smsgs);
    if (smsgs == NULL)
	adios (NULL, "unable to allocate folder storage");

    hi = mp->hghmsg + 1;
    interrupted = 0;
    for (msgnum = mp->lowsel;
	    msgnum <= mp->hghsel && !interrupted;
	    msgnum++)
	if (is_selected (mp, msgnum))
	    if (burst (smsgs, msgnum, inplace, quietsw, verbosw) != OK)
		break;

    free ((char *) smsgs);

    if (inplace)
	seq_setcur (mp, mp->lowsel);
    else
	if (hi <= mp->hghmsg)
	    seq_setcur (mp, hi);

    mp->msgflags |= MODIFIED;
    modified++;
}


static int
burst (struct Msg *smsgs, int msgnum, int inplace, int quietsw, int verbosw)
{
    int i, j, ld3, wasdlm, msgp;
    long pos;
    char c, buffer[BUFSIZ];
    register FILE *zp;

    ld3 = strlen (delim3);

    if (Msgs[msgnum].m_scanl) {
	free (Msgs[msgnum].m_scanl);
	Msgs[msgnum].m_scanl = NULL;
    }

    pos = ftell (zp = msh_ready (msgnum, 1));
    for (msgp = 0; msgp <= MAXFOLDER;) {
	while (fgets (buffer, sizeof buffer, zp) != NULL
		&& buffer[0] == '\n'
		&& pos < Msgs[msgnum].m_stop)
	    pos += (long) strlen (buffer);
	if (feof (zp) || pos >= Msgs[msgnum].m_stop)
	    break;
	fseek (zp, pos, SEEK_SET);
	smsgs[msgp].m_start = pos;

	for (c = 0;
		pos < Msgs[msgnum].m_stop
		&& fgets (buffer, sizeof buffer, zp) != NULL;
		c = buffer[0])
	    if (strncmp (buffer, delim3, ld3) == 0
		    && (msgp == 1 || c == '\n')
		    && peekc (zp) == '\n')
		break;
	    else
		pos += (long) strlen (buffer);

	wasdlm = strncmp (buffer, delim3, ld3) == 0;
	if (smsgs[msgp].m_start != pos)
	    smsgs[msgp++].m_stop = (c == '\n' && wasdlm) ? pos - 1 : pos;
	if (feof (zp) || pos >= Msgs[msgnum].m_stop) {
	    if (wasdlm)
		smsgs[msgp - 1].m_stop -= ((long) strlen (buffer) + 1);
	    break;
	}
	pos += (long) strlen (buffer);
    }

    switch (msgp--) {		/* toss "End of XXX Digest" */
	case 0: 
	    adios (NULL, "burst() botch -- you lose big");

	case 1: 
	    if (!quietsw)
		printf ("message %d not in digest format\n", msgnum);
	    return OK;

	default: 
	    if (verbosw)
		printf ("%d message%s exploded from digest %d\n",
			msgp, msgp != 1 ? "s" : "", msgnum);
	    break;
    }

    if ((i = msgp + mp->hghmsg) > MAXFOLDER) {
	advise (NULL, "more than %d messages", MAXFOLDER);
	return NOTOK;
    }
    if (!(mp = folder_realloc (mp, mp->lowoff, i)))
	adios (NULL, "unable to allocate folder storage");

    j = mp->hghmsg;
    mp->hghmsg += msgp;
    mp->nummsg += msgp;
    if (mp->hghsel > msgnum)
	mp->hghsel += msgp;

    if (inplace)
	for (i = mp->hghmsg; j > msgnum; i--, j--) {
	    if (verbosw)
		printf ("message %d becomes message %d\n", j, i);

	    Msgs[i].m_bboard_id = Msgs[j].m_bboard_id;
	    Msgs[i].m_top = Msgs[j].m_top;
	    Msgs[i].m_start = Msgs[j].m_start;
	    Msgs[i].m_stop = Msgs[j].m_stop;
	    Msgs[i].m_scanl = NULL;
	    if (Msgs[j].m_scanl) {
		free (Msgs[j].m_scanl);
		Msgs[j].m_scanl = NULL;
	    }
	    copy_msg_flags (mp, i, j);
	}

    if (Msgs[msgnum].m_bboard_id == 0)
	readid (msgnum);

    unset_selected (mp, msgnum);
    i = inplace ? msgnum + msgp : mp->hghmsg;
    for (j = msgp; j >= (inplace ? 0 : 1); i--, j--) {
	if (verbosw && i != msgnum)
	    printf ("message %d of digest %d becomes message %d\n",
		    j, msgnum, i);

	Msgs[i].m_bboard_id = Msgs[msgnum].m_bboard_id;
	Msgs[i].m_top = Msgs[j].m_top;
	Msgs[i].m_start = smsgs[j].m_start;
	Msgs[i].m_stop = smsgs[j].m_stop;
	Msgs[i].m_scanl = NULL;
	copy_msg_flags (mp, i, msgnum);
    }

    return OK;
}


#define FILE_SWITCHES \
    X("draft", 0, FIDRFT) \
    X("link", 0, FILINK) \
    X("nolink", 0, FINLINK) \
    X("preserve", 0, FIPRES) \
    X("nopreserve", 0, FINPRES) \
    X("src +folder", 0, FISRC) \
    X("file file", 0, FIFILE) \
    X("rmmproc program", 0, FIPROC) \
    X("normmproc", 0, FINPRC) \
    X("help", 0, FIHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(FILE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(FILE, fileswit);
#undef X


void
filecmd (char **args)
{
    int	linksw = 0, msgp = 0;
    int vecp = 1, i, msgnum;
    char *cp, buf[BUFSIZ];
    char *msgs[MAXARGS], *vec[MAXARGS];

    if (fmsh) {
	forkcmd (args, cmd_name);
	return;
    }

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (i = smatch (++cp, fileswit)) {
		case AMBIGSW: 
		    ambigsw (cp, fileswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case FIHELP: 
		    snprintf (buf, sizeof(buf), "%s +folder... [msgs] [switches]", cmd_name);
		    print_help (buf, fileswit, 1);
		    return;

		case FILINK:
		    linksw++;
		    continue;
		case FINLINK: 
		    linksw = 0;
		    continue;

		case FIPRES: 
		case FINPRES: 
		    continue;

		case FISRC: 
		case FIDRFT:
		case FIFILE: 
		case FIPROC:
		case FINPRC:
		    advise (NULL, "sorry, -%s not allowed!", fileswit[i].sw);
		    return;
	    }
	if (*cp == '+' || *cp == '@')
	    vec[vecp++] = cp;
	else
	    msgs[msgp++] = cp;
    }

    vec[0] = cmd_name;
    vec[vecp++] = "-file";
    vec[vecp] = NULL;
    if (!msgp)
	msgs[msgp++] = "cur";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    interrupted = 0;
    for (msgnum = mp->lowsel;
	    msgnum <= mp->hghsel && !interrupted;
	    msgnum++)
	if (is_selected (mp, msgnum))
	    if (process (msgnum, fileproc, vecp, vec)) {
		unset_selected (mp, msgnum);
		mp->numsel--;
	    }

    if (mp->numsel != mp->nummsg || linksw)
	seq_setcur (mp, mp->hghsel);
    if (!linksw)
	rmm ();
}


int
filehak (char **args)
{
    int	result, vecp = 0;
    char *cp, *cwd, *vec[MAXARGS];

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, fileswit)) {
		case AMBIGSW: 
		case UNKWNSW: 
		case FIHELP: 
		    return NOTOK;

		case FILINK:
		case FINLINK: 
		case FIPRES: 
		case FINPRES: 
		    continue;

		case FISRC: 
		case FIDRFT:
		case FIFILE: 
		    return NOTOK;
	    }
	if (*cp == '+' || *cp == '@')
	    vec[vecp++] = cp;
    }
    vec[vecp] = NULL;

    result = NOTOK;
    cwd = NULL;
    for (vecp = 0; (cp = vec[vecp]) && result == NOTOK; vecp++) {
	if (cwd == NULL)
	    cwd = getcpy (pwd ());
	chdir (m_maildir (""));
	cp = pluspath (cp);
	if (access (m_maildir (cp), F_OK) == NOTOK)
	    result = OK;
	free (cp);
    }
    if (cwd)
	chdir (cwd);

    return result;
}


#define FOLDER_SWITCHES \
    X("all", 0, FLALSW) \
    X("fast", 0, FLFASW) \
    X("nofast", 0, FLNFASW) \
    X("header", 0, FLHDSW) \
    X("noheader", 0, FLNHDSW) \
    X("pack", 0, FLPKSW) \
    X("nopack", 0, FLNPKSW) \
    X("recurse", 0, FLRCSW) \
    X("norecurse", 0, FLNRCSW) \
    X("total", 0, FLTLSW) \
    X("nototal", 0, FLNTLSW) \
    X("print", 0, FLPRSW) \
    X("push", 0, FLPUSW) \
    X("pop", 0, FLPOSW) \
    X("list", 0, FLLISW) \
    X("help", 0, FLHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(FOLDER);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(FOLDER, foldswit);
#undef X


void
foldcmd (char **args)
{
    int fastsw = 0, headersw = 0, packsw = 0;
    int hole, msgnum;
    char *cp, *folder = NULL, *msg = NULL;
    char buf[BUFSIZ], **vec = args;

    if (args == NULL)
	goto fast;

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, foldswit)) {
		case AMBIGSW: 
		    ambigsw (cp, foldswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case FLHELP: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [msg] [switches]", cmd_name);
		    print_help (buf, foldswit, 1);
		    return;

		case FLALSW:	/* not implemented */
		case FLRCSW: 
		case FLNRCSW: 
		case FLTLSW: 
		case FLNTLSW: 
		case FLPRSW:
		case FLPUSW:
		case FLPOSW:
		case FLLISW:
		    continue;

		case FLFASW: 
		    fastsw++;
		    continue;
		case FLNFASW: 
		    fastsw = 0;
		    continue;
		case FLHDSW: 
		    headersw++;
		    continue;
		case FLNHDSW: 
		    headersw = 0;
		    continue;
		case FLPKSW: 
		    packsw++;
		    continue;
		case FLNPKSW: 
		    packsw = 0;
		    continue;
	    }
	if (*cp == '+' || *cp == '@') {
	    if (folder) {
		advise (NULL, "only one folder at a time!\n");
		return;
	    }
	    else
		folder = fmsh ? pluspath (cp)
			    : cp + 1;
	}
	else
	    if (msg) {
		advise (NULL, "only one message at a time!\n");
		return;
	    }
	    else
		msg = cp;
    }

    if (folder) {
	if (*folder == 0) {
	    advise (NULL, "null folder names are not permitted");
	    return;
	}
	if (fmsh) {
	    if (access (m_maildir (folder), R_OK) == NOTOK) {
		advise (folder, "unable to read");
		return;
	    }
	}
	else {
	    strncpy (buf, folder, sizeof(buf));
	    if (expand (buf) == NOTOK)
		return;
	    folder = buf;
	    if (access (folder, R_OK) == NOTOK) {
		advise (folder, "unable to read");
		return;
	    }
	}
	m_reset ();

	if (fmsh)
	    fsetup (folder);
	else
	    setup (folder);
	readids (0);
	display_info (0);
    }

    if (msg) {
	if (!m_convert (mp, msg))
	    return;
	seq_setprev (mp);

	if (mp->numsel > 1) {
	    advise (NULL, "only one message at a time!");
	    return;
	}
	seq_setcur (mp, mp->hghsel);
    }

    if (packsw) {
	if (fmsh) {
	    forkcmd (vec, cmd_name);
	    return;
	}

	if (mp->lowoff > 1 && !(mp = folder_realloc (mp, 1, mp->hghmsg)))
	    adios (NULL, "unable to allocate folder storage");

	for (msgnum = mp->lowmsg, hole = 1; msgnum <= mp->hghmsg; msgnum++)
	    if (does_exist (mp, msgnum)) {
		if (msgnum != hole) {
		    Msgs[hole].m_bboard_id = Msgs[msgnum].m_bboard_id;
		    Msgs[hole].m_top = Msgs[msgnum].m_top;
		    Msgs[hole].m_start = Msgs[msgnum].m_start;
		    Msgs[hole].m_stop = Msgs[msgnum].m_stop;
		    Msgs[hole].m_scanl = NULL;
		    if (Msgs[msgnum].m_scanl) {
			free (Msgs[msgnum].m_scanl);
			Msgs[msgnum].m_scanl = NULL;
		    }
		    copy_msg_flags (mp, hole, msgnum);
		    if (mp->curmsg == msgnum)
			seq_setcur (mp, hole);
		}
		hole++;
	    }
	if (mp->nummsg > 0) {
	    mp->lowmsg = 1;
	    mp->hghmsg = hole - 1;
	}
	mp->msgflags |= MODIFIED;
	modified++;
    }

fast: ;
    if (fastsw)
	printf ("%s\n", fmsh ? fmsh : mp->foldpath);
    else {
	if (headersw)
	    printf ("\t\tFolder  %*s# of messages (%*srange%*s); cur%*smsg\n",
		DMAXFOLDER, "", DMAXFOLDER - 2, "", DMAXFOLDER - 2, "",
		DMAXFOLDER - 2, "");
	printf (args ? "%22s  " : "%s ", fmsh ? fmsh : mp->foldpath);

	/* check for empty folder */
	if (mp->nummsg == 0) {
	    printf ("has   no messages%*s",
		    mp->msgflags & OTHERS ? DMAXFOLDER * 2 + 4 : 0, "");
	} else {
	    printf ("has %*d message%s (%*d-%*d)",
		    DMAXFOLDER, mp->nummsg, mp->nummsg != 1 ? "s" : "",
		    DMAXFOLDER, mp->lowmsg, DMAXFOLDER, mp->hghmsg);
	    if (mp->curmsg >= mp->lowmsg
		    && mp->curmsg <= mp->hghmsg)
		printf ("; cur=%*d", DMAXFOLDER, mp->curmsg);
	}
	printf (".\n");
    }
}


#define FORW_SWITCHES \
    X("annotate", 0, FOANSW) \
    X("noannotate", 0, FONANSW) \
    X("draftfolder +folder", 0, FODFSW) \
    X("draftmessage msg", 0, FODMSW) \
    X("nodraftfolder", 0, FONDFSW) \
    X("editor editor", 0, FOEDTSW) \
    X("noedit", 0, FONEDSW) \
    X("filter filterfile", 0, FOFTRSW) \
    X("form formfile", 0, FOFRMSW) \
    X("format", 5, FOFTSW) \
    X("noformat", 7, FONFTSW) \
    X("inplace", 0, FOINSW) \
    X("noinplace", 0, FONINSW) \
    X("mime", 0, FOMISW) \
    X("nomime", 0, FONMISW) \
    X("whatnowproc program", 0, FOWHTSW) \
    X("nowhatnow", 0, FONWTSW) \
    X("help", 0, FOHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(FORW);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(FORW, forwswit);
#undef X


void
forwcmd (char **args)
{
    int	msgp = 0, vecp = 1, msgnum;
    char *cp, *filter = NULL, buf[BUFSIZ];
    char *msgs[MAXARGS], *vec[MAXARGS];
    char *tfile = NULL;
    char tmpfil[BUFSIZ];

    if (fmsh) {
	forkcmd (args, cmd_name);
	return;
    }

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, forwswit)) {
		case AMBIGSW: 
		    ambigsw (cp, forwswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case FOHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, forwswit, 1);
		    return;

		case FOANSW:	/* not implemented */
		case FONANSW: 
		case FOINSW: 
		case FONINSW: 
		case FOMISW: 
		case FONMISW: 
		    continue;

		case FONDFSW:
		case FONEDSW:
		case FONWTSW:
		    vec[vecp++] = --cp;
		    continue;

		case FOEDTSW: 
		case FOFRMSW: 
		case FODFSW:
		case FODMSW:
		case FOWHTSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    vec[vecp++] = cp;
		    continue;
		case FOFTRSW: 
		    if (!(filter = *args++) || *filter == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    continue;
		case FOFTSW: 
		    if (access (filter = myfilter, R_OK) == NOTOK) {
			advise (filter, "unable to read default filter file");
			return;
		    }
		    continue;
		case FONFTSW: 
		    filter = NULL;
		    continue;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    msgs[msgp++] = cp;
    }

					/* foil search of .mh_profile */
    snprintf (buf, sizeof(buf), "%sXXXXXX", invo_name);

    tfile = m_mktemp(buf, NULL, NULL);
    if (tfile == NULL) adios("forwcmd", "unable to create temporary file");
    strncpy (tmpfil, tfile, sizeof(tmpfil));
    vec[0] = tmpfil;

    vec[vecp++] = "-file";
    vec[vecp] = NULL;
    if (!msgp)
	msgs[msgp++] = "cur";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    if (filter) {
	strncpy (buf, filter, sizeof(buf));
	if (expand (buf) == NOTOK)
	    return;
	if (access (filter = getcpy (etcpath (buf)), R_OK) == NOTOK) {
	    advise (filter, "unable to read");
	    free (filter);
	    return;
	}
    }
    forw (cmd_name, filter, vecp, vec);
    seq_setcur (mp, mp->hghsel);
    if (filter)
	free (filter);
}


static void
forw (char *proc, char *filter, int vecp, char **vec)
{
    int i, child_id, msgnum, msgcnt;
    char tmpfil[BUFSIZ], *args[MAXARGS];
    FILE *out;
    char *tfile = NULL;

    tfile = m_mktemp2(NULL, invo_name, NULL, NULL);
    if (tfile == NULL) adios("forw", "unable to create temporary file");
    strncpy (tmpfil, tfile, sizeof(tmpfil));

    interrupted = 0;
    if (filter)
	switch (child_id = fork ()) {
	    case NOTOK: 
		advise ("fork", "unable to");
		return;

	    case OK: 		/* "trust me" */
		if (freopen (tmpfil, "w", stdout) == NULL) {
		    fprintf (stderr, "unable to create ");
		    perror (tmpfil);
		    _exit (1);
		}
		args[0] = r1bindex (mhlproc, '/');
		i = 1;
		args[i++] = "-forwall";
		args[i++] = "-form";
		args[i++] = filter;
		for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		    if (is_selected (mp, msgnum))
			args[i++] = getcpy (m_name (msgnum));
		args[i] = NULL;
		mhlsbr (i, args, mhl_action);
		scan_eom_action ((int (*) ()) 0);
		fclose (stdout);
		_exit (0);

	    default: 
		if (pidXwait (child_id, NULL))
		    interrupted++;
		break;
	}
    else {
	if ((out = fopen (tmpfil, "w")) == NULL) {
	    advise (tmpfil, "unable to create temporary file");
	    return;
	}

	msgcnt = 1;
	for (msgnum = mp->lowsel;
		msgnum <= mp->hghsel && !interrupted;
		msgnum++)
	    if (is_selected (mp, msgnum)) {
		fprintf (out, "\n\n-------");
		if (msgnum == mp->lowsel)
		    fprintf (out, " Forwarded Message%s",
			    mp->numsel > 1 ? "s" : "");
		else
		    fprintf (out, " Message %d", msgcnt);
		fprintf (out, "\n\n");
		copy_digest (msgnum, out);
		msgcnt++;
	    }

	fprintf (out, "\n\n------- End of Forwarded Message%s\n",
		mp->numsel > 1 ? "s" : "");
	fclose (out);
    }

    fflush (stdout);
    if (!interrupted)
	switch (child_id = fork ()) {
	    case NOTOK: 
		advise ("fork", "unable to");
		break;

	    case OK: 
		closefds (3);
		SIGNAL (SIGINT, istat);
		SIGNAL (SIGQUIT, qstat);

		vec[vecp++] = tmpfil;
		vec[vecp] = NULL;

		execvp (proc, vec);
		fprintf (stderr, "unable to exec ");
		perror (proc);
		_exit (1);

	    default: 
		pidXwait (child_id, NULL);
		break;
	}

    unlink (tmpfil);
}


static char *hlpmsg[] = {
    "The %s program emulates many of the commands found in the nmh",
    "system.  Instead of operating on nmh folders, commands to %s concern",
    "a single file.",
    "",
    "To see the list of commands available, just type a ``?'' followed by",
    "the RETURN key.  To find out what switches each command takes, type",
    "the name of the command followed by ``-help''.  To leave %s, use the",
    "``quit'' command.",
    "",
    "Although a lot of nmh commands are found in %s, not all are fully",
    "implemented.  %s will always recognize all legal switches for a",
    "given command though, and will let you know when you ask for an",
    "option that it is unable to perform.",
    "",
    "Running %s is fun, but using nmh from your shell is far superior.",
    "After you have familiarized yourself with the nmh style by using %s,",
    "you should try using nmh from the shell.  You can still use %s for",
    "message files that aren't in nmh format, such as BBoard files.",
    NULL
};


void
helpcmd (char **args)
{
    int i;
    NMH_UNUSED (args);

    for (i = 0; hlpmsg[i]; i++) {
	printf (hlpmsg[i], invo_name);
	putchar ('\n');
    }
}


#define MARK_SWITCHES \
    X("add", 0, MADDSW) \
    X("delete", 0, MDELSW) \
    X("list", 0, MLSTSW) \
    X("sequence name", 0, MSEQSW) \
    X("public", 0, MPUBSW) \
    X("nopublic", 0, MNPUBSW) \
    X("zero", 0, MZERSW) \
    X("nozero", 0, MNZERSW) \
    X("help", 0, MHELP) \
    X("debug", -5, MDBUGSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MARK);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MARK, markswit);
#undef X


void
markcmd (char **args)
{
    int addsw = 0, deletesw = 0, debugsw = 0;
    int listsw = 0, zerosw = 0;
    size_t seqp = 0;
    int msgp = 0, msgnum;
    char *cp, buf[BUFSIZ];
    char *seqs[NUMATTRS + 1], *msgs[MAXARGS];

    while ((cp = *args++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, markswit)) {
		case AMBIGSW: 
		    ambigsw (cp, markswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case MHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, markswit, 1);
		    return;

		case MADDSW: 
		    addsw++;
		    deletesw = listsw = 0;
		    continue;
		case MDELSW: 
		    deletesw++;
		    addsw = listsw = 0;
		    continue;
		case MLSTSW: 
		    listsw++;
		    addsw = deletesw = 0;
		    continue;

		case MSEQSW: 
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    if (seqp < NUMATTRS)
			seqs[seqp++] = cp;
		    else {
			advise (NULL, "only %d sequences allowed!", NUMATTRS);
			return;
		    }
		    continue;

		case MPUBSW: 	/* not implemented */
		case MNPUBSW: 
		    continue;

		case MDBUGSW: 
		    debugsw++;
		    continue;

		case MZERSW: 
		    zerosw++;
		    continue;
		case MNZERSW: 
		    zerosw = 0;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	} else {
	    msgs[msgp++] = cp;
	}
    }

    if (!addsw && !deletesw && !listsw) {
	if (seqp)
	    addsw++;
	else
	    if (debugsw)
		listsw++;
	    else {
		seqs[seqp++] = "unseen";
		deletesw++;
		zerosw = 0;
		if (!msgp)
		    msgs[msgp++] = "all";
	    }
    }

    if (!msgp)
	msgs[msgp++] = listsw ? "all" :"cur";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;

    if (debugsw) {
	printf ("invo_name=%s mypath=%s defpath=%s\n",
		invo_name, mypath, defpath);
	printf ("ctxpath=%s context flags=%s\n",
		ctxpath, snprintb (buf, sizeof(buf), (unsigned) ctxflags, DBITS));
	printf ("foldpath=%s flags=%s\n",
		mp->foldpath,
		snprintb (buf, sizeof(buf), (unsigned) mp->msgflags, FBITS));
	printf ("hghmsg=%d lowmsg=%d nummsg=%d curmsg=%d\n",
		mp->hghmsg, mp->lowmsg, mp->nummsg, mp->curmsg);
	printf ("lowsel=%d hghsel=%d numsel=%d\n",
		mp->lowsel, mp->hghsel, mp->numsel);
	printf ("lowoff=%d hghoff=%d\n", mp->lowoff, mp->hghoff);
    }

    if (seqp == 0 && (addsw || deletesw)) {
	advise (NULL, "-%s requires at least one -sequence argument",
		addsw ? "add" : "delete");
	return;
    }
    seqs[seqp] = NULL;

    if (addsw) {
	for (seqp = 0; seqs[seqp]; seqp++)
	    if (!seq_addsel (mp, seqs[seqp], 0, zerosw))
		return;
    }

    if (deletesw) {
	for (seqp = 0; seqs[seqp]; seqp++)
	    if (!seq_delsel (mp, seqs[seqp], 0, zerosw))
		return;
    }

    /* Listing messages in sequences */
    if (listsw) {
	if (seqp) {
	    /* list the given sequences */
	    for (seqp = 0; seqs[seqp]; seqp++)
		seq_print (mp, seqs[seqp]);
	} else {
	    /* else list them all */
	    seq_printall (mp);
	}

	interrupted = 0;
	if (debugsw)
	    for (msgnum = mp->lowsel;
		    msgnum <= mp->hghsel && !interrupted;
		    msgnum++)
		if (is_selected (mp, msgnum)) {
		    printf ("%*d: id=%d top=%d start=%ld stop=%ld %s\n",
			DMAXFOLDER,
			msgnum,
			Msgs[msgnum].m_bboard_id,
			Msgs[msgnum].m_top,
			(long) Msgs[msgnum].m_start,
			(long) Msgs[msgnum].m_stop,
			snprintb (buf, sizeof(buf),
				(unsigned) mp->msgstats[msgnum - mp->lowoff],
				seq_bits (mp)));
		    if (Msgs[msgnum].m_scanl)
			printf ("%s", Msgs[msgnum].m_scanl);
		}			    
    }
}


#define MHN_SWITCHES \
    X("auto", 0, MHNAUTOSW) \
    X("noauto", 0, MHNNAUTOSW) \
    X("debug", -5, MHNDEBUGSW) \
    X("form formfile", 4, MHNFORMSW) \
    X("headers", 0, MHNHEADSW) \
    X("noheaders", 0, MHNNHEADSW) \
    X("list", 0, MHNLISTSW) \
    X("nolist", 0, MHNNLISTSW) \
    X("part number", 0, MHNPARTSW) \
    X("realsize", 0, MHNSIZESW) \
    X("norealsize", 0, MHNNSIZESW) \
    X("rfc934mode", 0, MHNRFC934SW) \
    X("norfc934mode", 0, MHNNRFC934SW) \
    X("serialonly", 0, MHNSERIALSW) \
    X("noserialonly", 0, MHNNSERIALSW) \
    X("show", 0, MHNSHOWSW) \
    X("noshow", 0, MHNNSHOWSW) \
    X("store", 0, MHNSTORESW) \
    X("nostore", 0, MHNNSTORESW) \
    X("type content", 0, MHNTYPESW) \
    X("verbose", 0, MHNVERBSW) \
    X("noverbose", 0, MHNNVERBSW) \
    X("help", 0, MHNHELPSW) \
    X("moreproc program", -4, MHNPROGSW) \
    X("nomoreproc", -3, MHNNPROGSW) \
    X("length lines", -4, MHNLENSW) \
    X("width columns", -4, MHNWIDSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHN);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHN, mhnswit);
#undef X


void
mhncmd (char **args)
{
    int msgp = 0, vecp = 1;
    int msgnum;
    char *cp, buf[BUFSIZ];
    char *msgs[MAXARGS], *vec[MAXARGS];

    if (fmsh) {
	forkcmd (args, cmd_name);
	return;
    }
    while ((cp = *args++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, mhnswit)) {
		case AMBIGSW: 
		    ambigsw (cp, mhnswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case MHNHELPSW:
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, mhnswit, 1);
		    return;

		case MHNAUTOSW:
		case MHNNAUTOSW:
		case MHNDEBUGSW:
		case MHNHEADSW:
		case MHNNHEADSW:
		case MHNLISTSW:
		case MHNNLISTSW:
		case MHNSIZESW:
		case MHNNSIZESW:
		case MHNRFC934SW:
		case MHNNRFC934SW:
		case MHNSERIALSW:
		case MHNNSERIALSW:
		case MHNSHOWSW:
		case MHNNSHOWSW:
		case MHNSTORESW:
		case MHNNSTORESW:
		case MHNVERBSW:
		case MHNNVERBSW:
		case MHNNPROGSW:
		    vec[vecp++] = --cp;
		    continue;

		case MHNFORMSW:
		case MHNPARTSW:
		case MHNTYPESW:
		case MHNPROGSW:
		case MHNLENSW:
		case MHNWIDSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    vec[vecp++] = cp;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	} else {
	    msgs[msgp++] = cp;
	}
    }

    vec[0] = cmd_name;
    vec[vecp++] = "-file";
    vec[vecp] = NULL;
    if (!msgp)
	msgs[msgp++] = "cur";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    interrupted = 0;
    for (msgnum = mp->lowsel;
	    msgnum <= mp->hghsel && !interrupted;
	    msgnum++)
	if (is_selected (mp, msgnum))
	    if (process (msgnum, cmd_name, vecp, vec)) {
		unset_selected (mp, msgnum);
		mp->numsel--;
	    }

    seq_setcur (mp, mp->hghsel);
}


#define PACK_SWITCHES \
    X("file name", 0, PAFISW) \
    X("help", 0, PAHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(PACK);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(PACK, packswit);
#undef X

static int mbx_style = MMDF_FORMAT;

void
packcmd (char **args)
{
    int msgp = 0, md, msgnum;
    char *cp, *file = NULL;
    char buf[BUFSIZ], *msgs[MAXARGS];
    struct stat st;

    if (fmsh) {
	forkcmd (args, cmd_name);
	return;
    }

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, packswit)) {
		case AMBIGSW: 
		    ambigsw (cp, packswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case PAHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, packswit, 1);
		    return;

		case PAFISW: 
		    if (!(file = *args++) || *file == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    continue;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    msgs[msgp++] = cp;
    }

    if (!file)
	file = "./msgbox";
    file = path (file, TFILE);
    if (stat (file, &st) == NOTOK) {
	if (errno != ENOENT) {
	    advise (file, "error on file");
	    goto done_pack;
	}
	md = getanswer (cp = concat ("Create file \"", file, "\"? ", NULL));
	free (cp);
	if (!md)
	    goto done_pack;
    }

    if (!msgp)
	msgs[msgp++] = "all";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    goto done_pack;
    seq_setprev (mp);

    if ((md = mbx_open (file, mbx_style, getuid (), getgid (), m_gmprot ())) == NOTOK) {
	advise (file, "unable to open");
	goto done_pack;
    }
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected (mp, msgnum))
	    if (pack (file, md, msgnum) == NOTOK)
		break;
    mbx_close (file, md);

    if (mp->hghsel != mp->curmsg)
	seq_setcur (mp, mp->lowsel);

done_pack: ;
    free (file);
}


int
pack (char *mailbox, int md, int msgnum)
{
    register FILE *zp;

    if (Msgs[msgnum].m_bboard_id == 0)
	readid (msgnum);

    zp = msh_ready (msgnum, 1);
    return mbx_write (mailbox, md, zp, Msgs[msgnum].m_bboard_id,
	    0L, ftell (zp), Msgs[msgnum].m_stop, 1, 1);
}


int
packhak (char **args)
{
    int	result;
    char *cp, *file = NULL;

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, packswit)) {
		case AMBIGSW: 
		case UNKWNSW: 
		case PAHELP: 
		    return NOTOK;

		case PAFISW: 
		    if (!(file = *args++) || *file == '-') 
			return NOTOK;
		    continue;
	    }
	if (*cp == '+' || *cp == '@')
	    return NOTOK;
    }

    file = path (file ? file : "./msgbox", TFILE);
    result = access (file, F_OK) == NOTOK ? OK : NOTOK;
    free (file);

    return result;
}


#define PICK_SWITCHES \
    X("and", 0, PIANSW) \
    X("or", 0, PIORSW) \
    X("not", 0, PINTSW) \
    X("lbrace", 0, PILBSW) \
    X("rbrace", 0, PIRBSW) \
    X("cc  pattern", 0, PICCSW) \
    X("date  pattern", 0, PIDASW) \
    X("from  pattern", 0, PIFRSW) \
    X("search  pattern", 0, PISESW) \
    X("subject  pattern", 0, PISUSW) \
    X("to  pattern", 0, PITOSW) \
    X("-othercomponent  pattern", 15, PIOTSW) \
    X("after date", 0, PIAFSW) \
    X("before date", 0, PIBFSW) \
    X("datefield field", 5, PIDFSW) \
    X("sequence name", 0, PISQSW) \
    X("public", 0, PIPUSW) \
    X("nopublic", 0, PINPUSW) \
    X("zero", 0, PIZRSW) \
    X("nozero", 0, PINZRSW) \
    X("list", 0, PILISW) \
    X("nolist", 0, PINLISW) \
    X("help", 0, PIHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(PICK);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(PICK, pickswit);
#undef X


void
pickcmd (char **args)
{
    int zerosw = 1, msgp = 0;
    size_t seqp = 0;
    int vecp = 0, hi, lo, msgnum;
    char *cp, buf[BUFSIZ], *msgs[MAXARGS];
    char *seqs[NUMATTRS], *vec[MAXARGS];
    register FILE *zp;

    while ((cp = *args++)) {
	if (*cp == '-') {
	    if (*++cp == '-') {
		vec[vecp++] = --cp;
		goto pattern;
	    }
	    switch (smatch (cp, pickswit)) {
		case AMBIGSW: 
		    ambigsw (cp, pickswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case PIHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, pickswit, 1);
		    return;

		case PICCSW: 
		case PIDASW: 
		case PIFRSW: 
		case PISUSW: 
		case PITOSW: 
		case PIDFSW: 
		case PIAFSW: 
		case PIBFSW: 
		case PISESW: 
		    vec[vecp++] = --cp;
pattern: ;
		    if (!(cp = *args++)) {/* allow -xyz arguments */
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    vec[vecp++] = cp;
		    continue;
		case PIOTSW: 
		    advise (NULL, "internal error!");
		    return;
		case PIANSW: 
		case PIORSW: 
		case PINTSW: 
		case PILBSW: 
		case PIRBSW: 
		    vec[vecp++] = --cp;
		    continue;

		case PISQSW: 
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    if (seqp < NUMATTRS)
			seqs[seqp++] = cp;
		    else {
			advise (NULL, "only %d sequences allowed!", NUMATTRS);
			return;
		    }
		    continue;
		case PIZRSW: 
		    zerosw++;
		    continue;
		case PINZRSW: 
		    zerosw = 0;
		    continue;

		case PIPUSW: 	/* not implemented */
		case PINPUSW: 
		case PILISW: 
		case PINLISW: 
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    msgs[msgp++] = cp;
    }
    vec[vecp] = NULL;

    if (!msgp)
	msgs[msgp++] = "all";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    interrupted = 0;
    if (!pcompile (vec, NULL))
	return;

    lo = mp->lowsel;
    hi = mp->hghsel;

    for (msgnum = mp->lowsel;
	    msgnum <= mp->hghsel && !interrupted;
	    msgnum++)
	if (is_selected (mp, msgnum)) {
	    zp = msh_ready (msgnum, 1);
	    if (pmatches (zp, msgnum, fmsh ? 0L : Msgs[msgnum].m_start,
			fmsh ? 0L : Msgs[msgnum].m_stop)) {
		if (msgnum < lo)
		    lo = msgnum;
		if (msgnum > hi)
		    hi = msgnum;
	    }
	    else {
		unset_selected (mp, msgnum);
		mp->numsel--;
	    }
	}

    if (interrupted)
	return;

    mp->lowsel = lo;
    mp->hghsel = hi;

    if (mp->numsel <= 0) {
	advise (NULL, "no messages match specification");
	return;
    }

    seqs[seqp] = NULL;
    for (seqp = 0; seqs[seqp]; seqp++)
	if (!seq_addsel (mp, seqs[seqp], 0, zerosw))
	    return;

    printf ("%d hit%s\n", mp->numsel, mp->numsel == 1 ? "" : "s");
}


#define REPL_SWITCHES \
    X("annotate", 0, REANSW) \
    X("noannotate", 0, RENANSW) \
    X("cc type", 0, RECCSW) \
    X("nocc type", 0, RENCCSW) \
    X("draftfolder +folder", 0, REDFSW) \
    X("draftmessage msg", 0, REDMSW) \
    X("nodraftfolder", 0, RENDFSW) \
    X("editor editor", 0, REEDTSW) \
    X("noedit", 0, RENEDSW) \
    X("fcc +folder", 0, REFCCSW) \
    X("filter filterfile", 0, REFLTSW) \
    X("form formfile", 0, REFRMSW) \
    X("inplace", 0, REINSW) \
    X("noinplace", 0, RENINSW) \
    X("query", 0, REQUSW) \
    X("noquery", 0, RENQUSW) \
    X("whatnowproc program", 0, REWHTSW) \
    X("nowhatnow", 0, RENWTSW) \
    X("width columns", 0, REWIDSW) \
    X("help", 0, REHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(REPL);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(REPL, replswit);
#undef X


void
replcmd (char **args)
{
    int vecp = 1;
    char *cp, *msg = NULL;
    char buf[BUFSIZ], *vec[MAXARGS];

    if (fmsh) {
	forkcmd (args, cmd_name);
	return;
    }

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, replswit)) {
		case AMBIGSW: 
		    ambigsw (cp, replswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case REHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, replswit, 1);
		    return;

		case REANSW:	/* not implemented */
		case RENANSW: 
		case REINSW: 
		case RENINSW: 
		    continue;

		case REQUSW:
		case RENQUSW:
		case RENDFSW:
		case RENEDSW:
		case RENWTSW:
		    vec[vecp++] = --cp;
		    continue;

		case RECCSW: 
		case RENCCSW: 
		case REEDTSW: 
		case REFCCSW: 
		case REFLTSW:
		case REFRMSW: 
		case REWIDSW: 
		case REDFSW:
		case REDMSW:
		case REWHTSW:
		    vec[vecp++] = --cp;
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    vec[vecp++] = cp;
		    continue;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    if (msg) {
		advise (NULL, "only one message at a time!");
		return;
	    }
	    else
		msg = cp;
    }

    vec[0] = cmd_name;
    vec[vecp++] = "-file";
    vec[vecp] = NULL;
    if (!msg)
	msg = "cur";
    if (!m_convert (mp, msg))
	return;
    seq_setprev (mp);

    if (mp->numsel > 1) {
	advise (NULL, "only one message at a time!");
	return;
    }
    process (mp->hghsel, cmd_name, vecp, vec);
    seq_setcur (mp, mp->hghsel);
}


#define RMM_SWITCHES \
    X("help", 0, RMHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(RMM);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(RMM, rmmswit);
#undef X


void
rmmcmd (char **args)
{
    int	msgp = 0, msgnum;
    char *cp, buf[BUFSIZ], *msgs[MAXARGS];

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, rmmswit)) {
		case AMBIGSW: 
		    ambigsw (cp, rmmswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case RMHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, rmmswit, 1);
		    return;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    msgs[msgp++] = cp;
    }

    if (!msgp)
	msgs[msgp++] = "cur";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    rmm ();
}


static void
rmm (void)
{
    register int msgnum, vecp;
    register char *cp;
    char buffer[BUFSIZ], *vec[MAXARGS];

    if (fmsh) {
	if (rmmproc) {
	    if (mp->numsel > MAXARGS - 1) {
		advise (NULL, "more than %d messages for %s exec",
			MAXARGS - 1, rmmproc);
		return;
	    }
	    vecp = 0;
	    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		if (is_selected (mp, msgnum))
		    vec[vecp++] = getcpy (m_name (msgnum));
	    vec[vecp] = NULL;
	    forkcmd (vec, rmmproc);
	    for (vecp = 0; vec[vecp]; vecp++)
		free (vec[vecp]);
	}
	else
	    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
		if (is_selected (mp, msgnum)) {
		    strncpy (buffer, m_backup (cp = m_name (msgnum)), sizeof(buffer));
		    if (rename (cp, buffer) == NOTOK)
			admonish (buffer, "unable to rename %s to", cp);
		}
    }

    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected (mp, msgnum)) {
	    set_deleted (mp, msgnum);
	    unset_exists (mp, msgnum);
	}

    if ((mp->nummsg -= mp->numsel) <= 0) {
	if (fmsh)
	    admonish (NULL, "no messages remaining in +%s", fmsh);
	else
	    admonish (NULL, "no messages remaining in %s", mp->foldpath);
	mp->lowmsg = mp->hghmsg = mp->nummsg = 0;
    }
    if (mp->lowsel == mp->lowmsg) {
	for (msgnum = mp->lowmsg + 1; msgnum <= mp->hghmsg; msgnum++)
	    if (does_exist (mp, msgnum))
		break;
	mp->lowmsg = msgnum;
    }
    if (mp->hghsel == mp->hghmsg) {
	for (msgnum = mp->hghmsg - 1; msgnum >= mp->lowmsg; msgnum--)
	    if (does_exist (mp, msgnum))
		break;
	mp->hghmsg = msgnum;
    }

    mp->msgflags |= MODIFIED;
    modified++;
}


#define SCAN_SWITCHES \
    X("clear", 0, SCCLR) \
    X("noclear", 0, SCNCLR) \
    X("form formatfile", 0, SCFORM) \
    X("format string", 5, SCFMT) \
    X("header", 0, SCHEAD) \
    X("noheader", 0, SCNHEAD) \
    X("width columns", 0, SCWID) \
    X("help", 0, SCHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SCAN);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SCAN, scanswit);
#undef X


void
scancmd (char **args)
{
#define	equiv(a,b)	(a ? b && !strcmp (a, b) : !b)

    int clearsw = 0, headersw = 0, width = 0, msgp = 0;
    int msgnum, optim, state;
    char *cp, *form = NULL, *format = NULL;
    char buf[BUFSIZ], *nfs, *msgs[MAXARGS];
    register FILE *zp;
    static int s_optim = 0;
    static char *s_form = NULL, *s_format = NULL;

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, scanswit)) {
		case AMBIGSW: 
		    ambigsw (cp, scanswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case SCHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, scanswit, 1);
		    return;

		case SCCLR: 
		    clearsw++;
		    continue;
		case SCNCLR: 
		    clearsw = 0;
		    continue;
		case SCHEAD: 
		    headersw++;
		    continue;
		case SCNHEAD: 
		    headersw = 0;
		    continue;
		case SCFORM: 
		    if (!(form = *args++) || *form == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    format = NULL;
		    continue;
		case SCFMT: 
		    if (!(format = *args++) || *format == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    form = NULL;
		    continue;
		case SCWID: 
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    width = atoi (cp);
		    continue;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    msgs[msgp++] = cp;
    }

    if (!msgp)
	msgs[msgp++] = "all";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    /* Get new format string */
    nfs = new_fs (form, format, FORMAT);

    /* force scansbr to (re)compile format */
    if (scanl) {
	free (scanl);
	scanl = NULL;
    }

    if (s_optim == 0) {
	s_optim = optim = 1;
	s_form = form ? getcpy (form) : NULL;
	s_format = format ? getcpy (format) : NULL;

    }
    else
	optim = equiv (s_form, form) && equiv (s_format, format);

    interrupted = 0;
    for (msgnum = mp->lowsel;
	    msgnum <= mp->hghsel && !interrupted;
	    msgnum++)
	if (is_selected (mp, msgnum)) {
	    if (optim && Msgs[msgnum].m_scanl)
		printf ("%s", Msgs[msgnum].m_scanl);
	    else {

		zp = msh_ready (msgnum, 0);
		switch (state = scan (zp, msgnum, 0, nfs, width,
			msgnum == mp->curmsg,
			is_unseen (mp, msgnum),
			headersw ? (fmsh ? fmsh : mp->foldpath) : NULL,
			fmsh ? 0L : (long) (Msgs[msgnum].m_stop - Msgs[msgnum].m_start),
			1)) {
		    case SCNMSG:
		    case SCNENC:
		    case SCNERR:
			if (optim)
			    Msgs[msgnum].m_scanl = getcpy (scanl);
			break;

		    default:
			advise (NULL, "scan() botch (%d)", state);
			return;

		    case SCNEOF:
			printf ("%*d  empty\n", DMAXFOLDER, msgnum);
			break;
		    }
	    }
	    headersw = 0;
	}

    if (clearsw)
	clear_screen ();
}


#define SHOW_SWITCHES \
    X("draft", 5, SHDRAFT) \
    X("form formfile", 4, SHFORM) \
    X("moreproc program", 4, SHPROG) \
    X("nomoreproc", 3, SHNPROG) \
    X("length lines", 4, SHLEN) \
    X("width columns", 4, SHWID) \
    X("showproc program", 4, SHSHOW) \
    X("noshowproc", 3, SHNSHOW) \
    X("header", 4, SHHEAD) \
    X("noheader", 3, SHNHEAD) \
    X("help", 0, SHHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SHOW);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SHOW, showswit);
#undef X


void
showcmd (char **args)
{
    int	headersw = 1, nshow = 0, msgp = 0, vecp = 1;
    int mhl = 0, seqnum = -1, mode = 0, i, msgnum;
    char *cp, *proc = showproc, buf[BUFSIZ];
    char *msgs[MAXARGS], *vec[MAXARGS];

    if (!mh_strcasecmp (cmd_name, "next"))
	mode = 1;
    else
	if (!mh_strcasecmp (cmd_name, "prev"))
	    mode = -1;
    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (i = smatch (++cp, showswit)) {
		case AMBIGSW: 
		    ambigsw (cp, showswit);
		    return;
		case UNKWNSW: 
		case SHNPROG:
		    vec[vecp++] = --cp;
		    continue;
		case SHHELP: 
		    snprintf (buf, sizeof(buf), "%s %s[switches] [switches for showproc]",
			    cmd_name, mode ? NULL : "[msgs] ");
		    print_help (buf, showswit, 1);
		    return;

		case SHFORM: 
		case SHPROG:
		case SHLEN:
		case SHWID:
		    vec[vecp++] = --cp;
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    vec[vecp++] = cp;
		    continue;
		case SHHEAD: 
		    headersw++;
		    continue;
		case SHNHEAD: 
		    headersw = 0;
		    continue;
		case SHSHOW: 
		    if (!(proc = *args++) || *proc == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    nshow = 0;
		    continue;
		case SHNSHOW: 
		    nshow++;
		    continue;

		case SHDRAFT: 
		    advise (NULL, "sorry, -%s not allowed!", showswit[i].sw);
		    return;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    if (mode) {
		fprintf (stderr,
			"usage: %s [switches] [switches for showproc]\n",
			cmd_name);
		return;
	    }
	    else
		msgs[msgp++] = cp;
    }
    vec[vecp] = NULL;

    if (!msgp)
	msgs[msgp++] = mode > 0 ? "next" : mode < 0 ? "prev" : "cur";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    if (!nshow)
	for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	    if (is_selected (mp, msgnum) && is_nontext (msgnum)) {
		proc = showmimeproc;
		vec[vecp++] = "-file";
		vec[vecp] = NULL;
		goto finish;
	    }

    if (nshow)
	proc = catproc;
    else
	if (strcmp (showproc, "mhl") == 0) {
	    proc = mhlproc;
	    mhl++;
	}

finish: ;
    seqnum = seq_getnum (mp, "unseen");
    vec[0] = r1bindex (proc, '/');
    if (mhl) {
	msgp = vecp;
	for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	    if (is_selected (mp, msgnum)) {
		vec[vecp++] = getcpy (m_name (msgnum));
		if (seqnum != -1)
		    seq_delmsg (mp, "unseen", msgnum);
	    }
	vec[vecp] = NULL;
	if (mp->numsel == 1 && headersw)
	    show (mp->lowsel);
	mhlsbr (vecp, vec, mhl_action);
	scan_eom_action ((int (*)()) 0);
	while (msgp < vecp)
	    free (vec[msgp++]);
    } else {
	interrupted = 0;
	for (msgnum = mp->lowsel;
		msgnum <= mp->hghsel && !interrupted;
		msgnum++)
	    if (is_selected (mp, msgnum)) {
		switch (ask (msgnum)) {
		    case NOTOK: /* QUIT */
			break;

		    case OK: 	/* INTR */
			continue;

		    default:
			if (mp->numsel == 1 && headersw)
			    show (msgnum);
			if (nshow)
			    copy_message (msgnum, stdout);
			else
			    process (msgnum, proc, vecp, vec);

			if (seqnum != -1)
			    seq_delmsg (mp, "unseen", msgnum);
			continue;
		}
		break;
	    }
    }

    seq_setcur (mp, mp->hghsel);
}


static void
show (int msgnum)
{
    if (Msgs[msgnum].m_bboard_id == 0)
	readid (msgnum);

    printf ("(Message %d", msgnum);
    if (Msgs[msgnum].m_bboard_id > 0)
	printf (", %s: %d", BBoard_ID, Msgs[msgnum].m_bboard_id);
    printf (")\n");
}



static int
eom_action (int c)
{
    NMH_UNUSED (c);

    return (ftell (mhlfp) >= Msgs[mhlnum].m_stop);
}


static FILE *
mhl_action (char *name)
{
    int msgnum;

    if ((msgnum = m_atoi (name)) < mp->lowmsg
	    || msgnum > mp->hghmsg
	    || !does_exist (mp, msgnum))
	return NULL;
    mhlnum = msgnum;

    mhlfp = msh_ready (msgnum, 1);
    if (!fmsh)
	scan_eom_action (eom_action);

    return mhlfp;
}



static int
ask (int msgnum)
{
    char buf[BUFSIZ];

    if (mp->numsel == 1 || !interactive || redirected)
	return DONE;

    if (SOprintf ("Press <return> to list \"%d\"...", msgnum)) {
	if (mp->lowsel != msgnum)
	    printf ("\n\n\n");
	printf ("Press <return> to list \"%d\"...", msgnum);
    }
    fflush (stdout);
    buf[0] = 0;

    read (fileno (stdout), buf, sizeof buf);

    if (strchr(buf, '\n') == NULL)
	putchar ('\n');

    if (told_to_quit) {
	told_to_quit = interrupted = 0;
	return NOTOK;
    }
    if (interrupted) {
	interrupted = 0;
	return OK;
    }

    return DONE;
}


#include <h/mime.h>

static int
is_nontext (int msgnum)
{
    int	result, state;
    unsigned char *bp, *dp;
    char *cp;
    char buf[BUFSIZ], name[NAMESZ];
    FILE *fp;

    if (Msgs[msgnum].m_flags & MHNCHK)
	return (Msgs[msgnum].m_flags & MHNYES);
    Msgs[msgnum].m_flags |= MHNCHK;

    fp = msh_ready (msgnum, 1);

    for (;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld (&gstate, name, buf, &bufsz, fp)) {
	case FLD:
	case FLDPLUS:
	    /*
	     * Check Content-Type field
	     */
	    if (!mh_strcasecmp (name, TYPE_FIELD)) {
		int passno;
		char c;

		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld (&gstate, name, buf, &bufsz, fp);
		    cp = add (buf, cp);
		}
		bp = cp;
		passno = 1;

again:
		for (; isspace (*bp); bp++)
		    continue;
		if (*bp == '(') {
		    int	i;

		    for (bp++, i = 0;;) {
			switch (*bp++) {
			case '\0':
invalid:
			    result = 0;
			    goto out;
			case '\\':
			    if (*bp++ == '\0')
				goto invalid;
			    continue;
			case '(':
			    i++;
			    /* and fall... */
			default:
			    continue;
			case ')':
			    if (--i < 0)
				break;
			    continue;
			}
			break;
		    }
		}
		if (passno == 2) {
		    if (*bp != '/')
			goto invalid;
		    bp++;
		    passno = 3;
		    goto again;
		}
		for (dp = bp; istoken (*dp); dp++)
		    continue;
		c = *dp;
		*dp = '\0';
		if (!*bp)
		    goto invalid;
		if (passno > 1) {
		    if ((result = (mh_strcasecmp (bp, "plain") != 0)))
			goto out;
		    *dp = c;
		    for (dp++; isspace (*dp); dp++)
			continue;
		    if (*dp) {
			if ((result = !uprf (dp, "charset")))
			    goto out;
			dp += sizeof "charset" - 1;
			while (isspace (*dp))
			    dp++;
			if (*dp++ != '=')
			    goto invalid;
			while (isspace (*dp))
			    dp++;
			if (*dp == '"') {
			    if ((bp = strchr(++dp, '"')))
				*bp = '\0';
			} else {
			    for (bp = dp; *bp; bp++)
				if (isspace (*bp)) {
				    *bp = '\0';
				    break;
				}
			}
		    } else {
			/* Default character set */
			dp = "US-ASCII";
		    }
		    /* Check the character set */
		    result = !check_charset (dp, strlen (dp));
		} else {
		    if (!(result = (mh_strcasecmp (bp, "text") != 0))) {
			*dp = c;
			bp = dp;
			passno = 2;
			goto again;
		    }
		}
out:
		free (cp);
		if (result) {
		    Msgs[msgnum].m_flags |= MHNYES;
		    return result;
		}
		break;
	    }

	    /*
	     * Check Content-Transfer-Encoding field
	     */
	    if (!mh_strcasecmp (name, ENCODING_FIELD)) {
		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld (&gstate, name, buf, &bufsz, fp);
		    cp = add (buf, cp);
		}
		for (bp = cp; isspace (*bp); bp++)
		    continue;
		for (dp = bp; istoken (*dp); dp++)
		    continue;
		*dp = '\0';
		result = (mh_strcasecmp (bp, "7bit")
		       && mh_strcasecmp (bp, "8bit")
		       && mh_strcasecmp (bp, "binary"));

		free (cp);
		if (result) {
		    Msgs[msgnum].m_flags |= MHNYES;
		    return result;
		}
		break;
	    }

	    /*
	     * Just skip the rest of this header
	     * field and go to next one.
	     */
	    while (state == FLDPLUS) {
		bufsz = sizeof buf;
		state = m_getfld (&gstate, name, buf, &bufsz, fp);
	    }
	    break;

	    /*
	     * We've passed the message header,
	     * so message is just text.
	     */
	default:
	    return 0;
	}
    }
}


#define SORT_SWITCHES \
    X("datefield field", 0, SODATE) \
    X("textfield field", 0, SOSUBJ) \
    X("notextfield", 0, SONSUBJ) \
    X("limit days", 0, SOLIMT) \
    X("nolimit", 0, SONLIMT) \
    X("verbose", 0, SOVERB) \
    X("noverbose", 0, SONVERB) \
    X("help", 0, SOHELP) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(SORT);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(SORT, sortswit);
#undef X


void
sortcmd (char **args)
{
    int msgp = 0, msgnum;
    char *cp, *datesw = NULL, *subjsw = NULL;
    char buf[BUFSIZ], *msgs[MAXARGS];
    struct tws tb;

    if (fmsh) {
	forkcmd (args, cmd_name);
	return;
    }

    while ((cp = *args++)) {
	if (*cp == '-')
	    switch (smatch (++cp, sortswit)) {
		case AMBIGSW: 
		    ambigsw (cp, sortswit);
		    return;
		case UNKWNSW: 
		    fprintf (stderr, "-%s unknown\n", cp);
		    return;
		case SOHELP: 
		    snprintf (buf, sizeof(buf), "%s [msgs] [switches]", cmd_name);
		    print_help (buf, sortswit, 1);
		    return;

		case SODATE: 
		    if (datesw) {
			advise (NULL, "only one date field at a time!");
			return;
		    }
		    if (!(datesw = *args++) || *datesw == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    continue;

		case SOSUBJ:
		    if (subjsw) {
			advise (NULL, "only one text field at a time!");
			return;
		    }
		    if (!(subjsw = *args++) || *subjsw == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		    continue;
		case SONSUBJ:
		    subjsw = (char *)0;
		    continue;

		case SOLIMT:		/* too hard */
		    if (!(cp = *args++) || *cp == '-') {
			advise (NULL, "missing argument to %s", args[-2]);
			return;
		    }
		case SONLIMT:
		case SOVERB: 		/* not implemented */
		case SONVERB: 
		    continue;
	    }
	if (*cp == '+' || *cp == '@') {
	    advise (NULL, "sorry, no folders allowed!");
	    return;
	}
	else
	    msgs[msgp++] = cp;
    }

    if (!msgp)
	msgs[msgp++] = "all";
    if (!datesw)
	datesw = "Date";
    for (msgnum = 0; msgnum < msgp; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    return;
    seq_setprev (mp);

    twscopy (&tb, dlocaltimenow ());

    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (Msgs[msgnum].m_scanl) {
	    free (Msgs[msgnum].m_scanl);
	    Msgs[msgnum].m_scanl = NULL;
	}
	if (is_selected (mp, msgnum)) {
	    if (get_fields (datesw, subjsw, msgnum, &Msgs[msgnum]))
		twscopy (&Msgs[msgnum].m_tb,
			msgnum != mp->lowsel ? &Msgs[msgnum - 1].m_tb : &tb);
	}
	else			/* m_scaln is already NULL */
	    twscopy (&Msgs[msgnum].m_tb, &tb);
	Msgs[msgnum].m_stats = mp->msgstats[msgnum - mp->lowoff];
	if (mp->curmsg == msgnum)
	    Msgs[msgnum].m_stats |= CUR;
    }

    qsort ((char *) &Msgs[mp->lowsel], mp->hghsel - mp->lowsel + 1,
	   sizeof(struct Msg), (qsort_comp) (subjsw ? subsort : msgsort));

    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
	if (subjsw && Msgs[msgnum].m_scanl) {
	    free (Msgs[msgnum].m_scanl);	/* from subjsort */
	    Msgs[msgnum].m_scanl = NULL;
	}
	mp->msgstats[msgnum - mp->lowoff] = Msgs[msgnum].m_stats & ~CUR;
	if (Msgs[msgnum].m_stats & CUR)
	    seq_setcur (mp, msgnum);
    }
	    
    mp->msgflags |= MODIFIED;
    modified++;
}


/* 
 * get_fields - parse message, and get date and subject if needed.
 * We'll use the msgp->m_tb tws struct for the date, and overload
 * the msgp->m_scanl field with our subject string.
 */
static int
get_fields (char *datesw, char *subjsw, int msgnum, struct Msg *msgp)
{
    int	state, gotdate = 0;
    char *bp, buf[BUFSIZ], name[NAMESZ];
    struct tws *tw = (struct tws *) 0;
    register FILE *zp;

    zp = msh_ready (msgnum, 0);

    for (;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld (&gstate, name, buf, &bufsz, zp)) {
	    case FLD: 
	    case FLDPLUS: 
		if (!mh_strcasecmp (name, datesw)) {
		    bp = getcpy (buf);
		    while (state == FLDPLUS) {
			bufsz = sizeof buf;
			state = m_getfld (&gstate, name, buf, &bufsz, zp);
			bp = add (buf, bp);
		    }
		    if ((tw = dparsetime (bp)) == NULL)
			admonish (NULL,
				"unable to parse %s field in message %d",
				datesw, msgnum);
		    else
			twscopy (&(msgp->m_tb), tw);
		    free (bp);
		    if (!subjsw)	/* not using this, or already done */
			break;		/* all done! */
		    gotdate++;
		}
		else if (subjsw && !mh_strcasecmp(name, subjsw)) {
		    bp = getcpy (buf);
		    while (state == FLDPLUS) {
			bufsz = sizeof buf;
			state = m_getfld (&gstate, name, buf, &bufsz, zp);
			bp = add (buf, bp);
		    }
		    msgp->m_scanl = sosmash(subjsw, bp);
		    if (gotdate)
			break;		/* date done so we're done */
		    else
			subjsw = (char *)0;/* subject done, need date */
		} else {
		    while (state == FLDPLUS) {	/* flush this one */
			bufsz = sizeof buf;
			state = m_getfld (&gstate, name, buf, &bufsz, zp);
		    }
		}
		continue;

	    case BODY: 
	    case FILEEOF: 
		break;

	    case LENERR: 
	    case FMTERR: 
		admonish (NULL, "format error in message %d", msgnum);
		if (msgp->m_scanl) {	/* this might need free'd */
		    free (msgp->m_scanl); /* probably can't use subj anyway */
		    msgp->m_scanl = NULL;
		}
		return NOTOK;

	    default: 
		adios (NULL, "internal error -- you lose");
	}
	break;
    }
    if (tw)
	return OK;	/* not an error if subj not found */

    admonish (NULL, "no %s field in message %d", datesw, msgnum);
    return NOTOK;	/* NOTOK means use some other date */
}


/*
 * sort routines
 */

static int
msgsort (struct Msg *a, struct Msg *b)
{
    return twsort (&a->m_tb, &b->m_tb);
}


static int
subsort (struct Msg *a, struct Msg *b)
{
	register int i;

	if (a->m_scanl && b->m_scanl)
	    if ((i = strcmp (a->m_scanl, b->m_scanl)))
		return (i);

	return twsort (&a->m_tb, &b->m_tb);
}


/*
 * try to make the subject "canonical": delete leading "re:", everything
 * but letters & smash letters to lower case. 
 */
static char *
sosmash (char *subj, char *s)
{
    register char *cp, *dp;
    register unsigned char c;

    if (s) {
	cp = s;
	dp = s;	/* dst pointer */
	if (!mh_strcasecmp (subj, "subject"))
	    while ((c = *cp)) {
		if (! isspace(c)) {
		    if(uprf(cp, "re:"))
			cp += 2;
		    else {
			if (isalnum(c))
			    *dp++ = isupper(c) ? tolower(c) : c;
			break;
		    }
		}
		cp++;
	    }
	while ((c = *cp++)) {
	    if (isalnum(c))
		*dp++ = isupper(c) ? tolower(c) : c;

	}
	*dp = '\0';
    }
    return s;
}


static int
process (int msgnum, char *proc, int vecp, char **vec)
{
    int	child_id, status;
    char tmpfil[BUFSIZ];
    FILE *out;
    char *cp;

    if (fmsh) {
	strncpy (tmpfil, m_name (msgnum), sizeof(tmpfil));
	context_del (pfolder);
	context_replace (pfolder, fmsh);/* update current folder   */
	seq_save (mp);
	context_save ();		/* save the context file   */
	goto ready;
    }

    cp = m_mktemp(invo_name, NULL, &out);
    if (cp == NULL) {
        /* Try again, but try to create under /tmp */
	int olderr = errno;
        cp = m_mktemp2(NULL, invo_name, NULL, &out);
        if (cp == NULL) {
	    errno = olderr;
	    advise (NULL, "unable to create temporary file");
	    return NOTOK;
	}
    }
    copy_message (msgnum, out);
    fclose (out);
    strncpy(tmpfil, cp, sizeof(tmpfil));

ready: ;
    fflush (stdout);
    switch (child_id = fork ()) {
	case NOTOK: 
	    advise ("fork", "unable to");
	    status = NOTOK;
	    break;
	    
	case OK: 
	    closefds (3);
	    SIGNAL (SIGINT, istat);
	    SIGNAL (SIGQUIT, qstat);

	    vec[vecp++] = tmpfil;
	    vec[vecp] = NULL;

	    execvp (proc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (proc);
	    _exit (1);

	default: 
	    status = pidXwait (child_id, NULL);
	    break;
    }

    if (!fmsh)
	unlink (tmpfil);
    return status;
}


static void
copy_message (int msgnum, FILE *out)
{
    long pos;
    static char buffer[BUFSIZ];
    register FILE *zp;

    zp = msh_ready (msgnum, 1);
    if (fmsh) {
	while (fgets (buffer, sizeof buffer, zp) != NULL) {
	    fputs (buffer, out);
	    if (interrupted && out == stdout)
		break;
	}
    }
    else {
	pos = ftell (zp);
	while (fgets (buffer, sizeof buffer, zp) != NULL
		&& pos < Msgs[msgnum].m_stop) {
	    fputs (buffer, out);
	    pos += (long) strlen (buffer);
	    if (interrupted && out == stdout)
		break;
	}
    }
}


static void
copy_digest (int msgnum, FILE *out)
{
    char c;
    long pos = 0L;
    static char buffer[BUFSIZ];
    register FILE *zp;

    c = '\n';
    zp = msh_ready (msgnum, 1);
    if (!fmsh)
	pos = ftell (zp);
    while (fgets (buffer, sizeof buffer, zp) != NULL
	    && !fmsh && pos < Msgs[msgnum].m_stop) {
	if (c == '\n' && *buffer == '-')
	    fputc (' ', out);
	fputs (buffer, out);
	c = buffer[strlen (buffer) - 1];
	if (!fmsh)
	    pos += (long) strlen (buffer);
	if (interrupted && out == stdout)
	    break;
    }
}
