/* picksbr.c -- routines to help pick along...
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/tws.h>
#include <h/picksbr.h>
#include <h/utils.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#define PARSE_SWITCHES \
    X("and", 0, PRAND) \
    X("or", 0, PROR) \
    X("not", 0, PRNOT) \
    X("lbrace", 0, PRLBR) \
    X("rbrace", 0, PRRBR) \
    X("cc  pattern", 0, PRCC) \
    X("date  pattern", 0, PRDATE) \
    X("from  pattern", 0, PRFROM) \
    X("search  pattern", 0, PRSRCH) \
    X("subject  pattern", 0, PRSUBJ) \
    X("to  pattern", 0, PRTO) \
    X("-othercomponent  pattern", 15, PROTHR) \
    X("after date", 0, PRAFTR) \
    X("before date", 0, PRBEFR) \
    X("datefield field", 5, PRDATF) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(PARSE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(PARSE, parswit);
#undef X

/* DEFINITIONS FOR PATTERN MATCHING */

/*
 * We really should be using re_comp() and re_exec() here.  Unfortunately,
 * pick advertises that lowercase characters matches characters of both
 * cases.  Since re_exec() doesn't exhibit this behavior, we are stuck
 * with this version.  Furthermore, we need to be able to save and restore
 * the state of the pattern matcher in order to do things "efficiently".
 *
 * The matching power of this algorithm isn't as powerful as the re_xxx()
 * routines (no \(xxx\) and \n constructs).  Such is life.
 */

#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11

#define	STAR	01

#define LBSIZE  NMH_BUFSIZ
#define	ESIZE	1024


static char linebuf[LBSIZE + 1];
static char decoded_linebuf[LBSIZE + 1];

/* the magic array for case-independence */
static unsigned char cc[] = {
	0000,0001,0002,0003,0004,0005,0006,0007,
	0010,0011,0012,0013,0014,0015,0016,0017,
	0020,0021,0022,0023,0024,0025,0026,0027,
	0030,0031,0032,0033,0034,0035,0036,0037,
	0040,0041,0042,0043,0044,0045,0046,0047,
	0050,0051,0052,0053,0054,0055,0056,0057,
	0060,0061,0062,0063,0064,0065,0066,0067,
	0070,0071,0072,0073,0074,0075,0076,0077,
	0100,0141,0142,0143,0144,0145,0146,0147,
	0150,0151,0152,0153,0154,0155,0156,0157,
	0160,0161,0162,0163,0164,0165,0166,0167,
	0170,0171,0172,0133,0134,0135,0136,0137,
	0140,0141,0142,0143,0144,0145,0146,0147,
	0150,0151,0152,0153,0154,0155,0156,0157,
	0160,0161,0162,0163,0164,0165,0166,0167,
	0170,0171,0172,0173,0174,0175,0176,0177,

	0200,0201,0202,0203,0204,0205,0206,0207,
	0210,0211,0212,0213,0214,0215,0216,0217,
	0220,0221,0222,0223,0224,0225,0226,0227,
	0230,0231,0232,0233,0234,0235,0236,0237,
	0240,0241,0242,0243,0244,0245,0246,0247,
	0250,0251,0252,0253,0254,0255,0256,0257,
	0260,0261,0262,0263,0264,0265,0266,0267,
	0270,0271,0272,0273,0274,0275,0276,0277,
	0300,0301,0302,0303,0304,0305,0306,0307,
	0310,0311,0312,0313,0314,0315,0316,0317,
	0320,0321,0322,0323,0324,0325,0326,0327,
	0330,0331,0332,0333,0334,0335,0336,0337,
	0340,0341,0342,0343,0344,0345,0346,0347,
	0350,0351,0352,0353,0354,0355,0356,0357,
	0360,0361,0362,0363,0364,0365,0366,0367,
	0370,0371,0372,0373,0374,0375,0376,0377,
};

/*
 * DEFINITIONS FOR NEXUS
 */

#define	nxtarg()	(*argp ? *argp++ : NULL)
#define	prvarg()	argp--

#define	pinform		if (!talked++) inform

struct nexus {
    int (*n_action)();

    union {
	/* for {OR,AND,NOT}action */
	struct {
	    struct nexus *un_L_child;
	    struct nexus *un_R_child;
	} st1;

	/* for GREPaction */
	struct {
	    int   un_header;
	    int   un_circf;
	    char  un_expbuf[ESIZE];
	    char *un_patbuf;
	} st2;

	/* for TWSaction */
	struct {
	    char *un_datef;
	    int   un_after;
	    struct tws un_tws;
	} st3;
    } un;
};

#define	n_L_child un.st1.un_L_child
#define	n_R_child un.st1.un_R_child

#define	n_header un.st2.un_header
#define	n_circf	 un.st2.un_circf
#define	n_expbuf un.st2.un_expbuf
#define	n_patbuf un.st2.un_patbuf

#define	n_datef	 un.st3.un_datef
#define	n_after	 un.st3.un_after
#define	n_tws	 un.st3.un_tws

static int talked;

static char *datesw;
static char **argp;

static struct nexus *head;

/*
 * prototypes for date routines
 */
static struct tws *tws_parse(char *, int);
static struct tws *tws_special(char *);

/*
 * static prototypes
 */
static void PRaction(struct nexus *, int);
static int gcompile(struct nexus *, char *);
static int advance(char *, char *);
static int cclass(unsigned char *, int, int);
static int tcompile(char *, struct tws *, int);

static struct nexus *parse(void);
static struct nexus *nexp1(void);
static struct nexus *nexp2(void);
static struct nexus *nexp3(void);
static struct nexus *newnexus(int (*)());

static int ORaction();
static int ANDaction();
static int NOTaction();
static int GREPaction();
static int TWSaction();


int
pcompile (char **vec, char *date)
{
    argp = vec;
    if ((datesw = date) == NULL)
	datesw = "date";
    talked = 0;

    if ((head = parse ()) == NULL)
        return !talked;

    if (*argp) {
	inform("%s unexpected", *argp);
	return 0;
    }

    return 1;
}


static struct nexus *
parse (void)
{
    char  *cp;
    struct nexus *n, *o;

    if ((n = nexp1 ()) == NULL || (cp = nxtarg ()) == NULL)
	return n;

    if (*cp != '-') {
	pinform("%s unexpected", cp);
	return NULL;
    }

    if (*++cp == '-')
	goto header;
    switch (smatch (cp, parswit)) {
	case AMBIGSW: 
	    ambigsw (cp, parswit);
	    talked++;
	    return NULL;
	case UNKWNSW: 
	    fprintf (stderr, "-%s unknown\n", cp);
	    talked++;
	    return NULL;

	case PROR: 
	    o = newnexus (ORaction);
	    o->n_L_child = n;
	    if ((o->n_R_child = parse ()))
		return o;
	    pinform("missing disjunctive");
	    free (o);
	    return NULL;

header: ;
	default: 
	    prvarg ();
	    return n;
    }
}

static struct nexus *
nexp1 (void)
{
    char *cp;
    struct nexus *n, *o;

    if ((n = nexp2 ()) == NULL || (cp = nxtarg ()) == NULL)
	return n;

    if (*cp != '-') {
	pinform("%s unexpected", cp);
	free (n);
	return NULL;
    }

    if (*++cp == '-')
	goto header;
    switch (smatch (cp, parswit)) {
	case AMBIGSW: 
	    ambigsw (cp, parswit);
	    talked++;
	    free (n);
	    return NULL;
	case UNKWNSW: 
	    fprintf (stderr, "-%s unknown\n", cp);
	    talked++;
	    free (n);
	    return NULL;

	case PRAND: 
	    o = newnexus (ANDaction);
	    o->n_L_child = n;
	    if ((o->n_R_child = nexp1 ()))
		return o;
	    pinform("missing conjunctive");
	    free (o);
	    return NULL;

header: ;
	default: 
	    prvarg ();
	    return n;
    }
}


static struct nexus *
nexp2 (void)
{
    char *cp;
    struct nexus *n;

    if ((cp = nxtarg ()) == NULL)
	return NULL;

    if (*cp != '-') {
	prvarg ();
	return nexp3 ();
    }

    if (*++cp == '-')
	goto header;
    switch (smatch (cp, parswit)) {
	case AMBIGSW: 
	    ambigsw (cp, parswit);
	    talked++;
	    return NULL;
	case UNKWNSW: 
	    fprintf (stderr, "-%s unknown\n", cp);
	    talked++;
	    return NULL;

	case PRNOT: 
	    n = newnexus (NOTaction);
	    if ((n->n_L_child = nexp3 ()))
		return n;
	    pinform("missing negation");
	    free (n);
	    return NULL;

header: ;
	default: 
	    prvarg ();
	    return nexp3 ();
    }
}

static struct nexus *
nexp3 (void)
{
    int i;
    char *cp, *dp;
    char buffer[BUFSIZ], temp[64];
    struct nexus *n;

    if ((cp = nxtarg ()) == NULL)
	return NULL;

    if (*cp != '-') {
	pinform("%s unexpected", cp);
	return NULL;
    }

    if (*++cp == '-') {
	dp = ++cp;
	goto header;
    }
    switch (i = smatch (cp, parswit)) {
	case AMBIGSW: 
	    ambigsw (cp, parswit);
	    talked++;
	    return NULL;
	case UNKWNSW: 
	    fprintf (stderr, "-%s unknown\n", cp);
	    talked++;
	    return NULL;

	case PRLBR: 
	    if ((n = parse ()) == NULL) {
		pinform("missing group");
		return NULL;
	    }
	    if ((cp = nxtarg ()) == NULL) {
		pinform("missing -rbrace");
		return NULL;
	    }
	    if (*cp++ == '-' && smatch (cp, parswit) == PRRBR)
		return n;
	    pinform("%s unexpected", --cp);
	    return NULL;

	default: 
	    prvarg ();
	    return NULL;

	case PRCC: 
	case PRDATE: 
	case PRFROM: 
	case PRTO: 
	case PRSUBJ: 
	    strncpy(temp, parswit[i].sw, sizeof(temp));
	    temp[sizeof(temp) - 1] = '\0';
	    dp = *brkstring (temp, " ", NULL);
    header: ;
	    if (!(cp = nxtarg ())) {/* allow -xyz arguments */
		pinform("missing argument to %s", argp[-2]);
		return NULL;
	    }
	    n = newnexus (GREPaction);
	    n->n_header = 1;
	    snprintf (buffer, sizeof(buffer), "^%s[ \t]*:.*%s", dp, cp);
	    dp = buffer;
	    goto pattern;

	case PRSRCH: 
	    n = newnexus (GREPaction);
	    n->n_header = 0;
	    if (!(cp = nxtarg ())) {/* allow -xyz arguments */
		pinform("missing argument to %s", argp[-2]);
		free (n);
		return NULL;
	    }
	    dp = cp;
    pattern: ;
	    if (!gcompile (n, dp)) {
		pinform("pattern error in %s %s", argp[-2], cp);
		free (n);
		return NULL;
	    }
	    n->n_patbuf = mh_xstrdup(dp);
	    return n;

	case PROTHR: 
	    pinform("internal error!");
	    return NULL;

	case PRDATF: 
	    if (!(datesw = nxtarg ()) || *datesw == '-') {
		pinform("missing argument to %s", argp[-2]);
		return NULL;
	    }
	    return nexp3 ();

	case PRAFTR: 
	case PRBEFR: 
	    if (!(cp = nxtarg ())) {/* allow -xyz arguments */
		pinform("missing argument to %s", argp[-2]);
		return NULL;
	    }
	    n = newnexus (TWSaction);
	    n->n_datef = datesw;
	    if (!tcompile (cp, &n->n_tws, n->n_after = i == PRAFTR)) {
		pinform("unable to parse %s %s", argp[-2], cp);
		free (n);
		return NULL;
	    }
	    return n;
    }
}


static struct nexus *
newnexus (int (*action)())
{
    struct nexus *p;

    NEW0(p);
    p->n_action = action;
    return p;
}


#define	args(a)	a, fp, msgnum, start, stop
#define	params	args (n)
#define	plist	\
	    struct nexus  *n; \
	    FILE *fp; \
	    int	msgnum; \
	    long    start, \
		    stop;

int
pmatches (FILE *fp, int msgnum, long start, long stop, int debug)
{
    if (!head)
	return 1;

    if (!talked++ && debug)
	PRaction (head, 0);

    return (*head->n_action) (args (head));
}


static void
PRaction (struct nexus *n, int level)
{
    int i;

    for (i = 0; i < level; i++)
	fprintf (stderr, "| ");

    if (n->n_action == ORaction) {
	fprintf (stderr, "OR\n");
	PRaction (n->n_L_child, level + 1);
	PRaction (n->n_R_child, level + 1);
	return;
    }
    if (n->n_action == ANDaction) {
	fprintf (stderr, "AND\n");
	PRaction (n->n_L_child, level + 1);
	PRaction (n->n_R_child, level + 1);
	return;
    }
    if (n->n_action == NOTaction) {
	fprintf (stderr, "NOT\n");
	PRaction (n->n_L_child, level + 1);
	return;
    }
    if (n->n_action == GREPaction) {
	fprintf (stderr, "PATTERN(%s) %s\n",
		n->n_header ? "header" : "body", n->n_patbuf);
	return;
    }
    if (n->n_action == TWSaction) {
	fprintf (stderr, "TEMPORAL(%s) %s: %s\n",
		n->n_after ? "after" : "before", n->n_datef,
		dasctime (&n->n_tws, TW_NULL));
	return;
    }
    fprintf (stderr, "UNKNOWN(0x%x)\n",
	     (unsigned int)(unsigned long) (*n->n_action));
}


static int
ORaction (params)
plist
{
    if ((*n->n_L_child->n_action) (args (n->n_L_child)))
	return 1;
    return (*n->n_R_child->n_action) (args (n->n_R_child));
}


static int
ANDaction (params)
plist
{
    if (!(*n->n_L_child->n_action) (args (n->n_L_child)))
	return 0;
    return (*n->n_R_child->n_action) (args (n->n_R_child));
}


static int
NOTaction (params)
plist
{
    return (!(*n->n_L_child->n_action) (args (n->n_L_child)));
}


static int
gcompile (struct nexus *n, char *astr)
{
    int c;
    int cclcnt;
    unsigned char *ep, *dp, *sp, *lastep = 0;

    dp = (ep = (unsigned char *) n->n_expbuf) + sizeof n->n_expbuf;
    sp = (unsigned char *) astr;
    if (*sp == '^') {
	n->n_circf = 1;
	sp++;
    }
    else
	n->n_circf = 0;
    for (;;) {
	if (ep >= dp)
	    goto cerror;
	if ((c = *sp++) != '*')
	    lastep = ep;
	switch (c) {
	    case '\0': 
		*ep++ = CEOF;
		return 1;

	    case '.': 
		*ep++ = CDOT;
		continue;

	    case '*': 
		if (lastep == 0)
		    goto defchar;
		*lastep |= STAR;
		continue;

	    case '$': 
		if (*sp != '\0')
		    goto defchar;
		*ep++ = CDOL;
		continue;

	    case '[': 
		*ep++ = CCL;
		*ep++ = 0;
		cclcnt = 0;
		if ((c = *sp++) == '^') {
		    c = *sp++;
		    ep[-2] = NCCL;
		}
		if (c == '-') {
		    *ep++ = c;
		    cclcnt++;
		    c = *sp++;
		}
		do {
		    if (c == '-' && *sp != '\0' && *sp != ']') {
			for (c = ep[-1]+1; c < *sp; c++) {
		    	    *ep++ = c;
		    	    cclcnt++;
			    if (c == '\0' || ep >= dp)
				goto cerror;
			}
		    } else {
			*ep++ = c;
			cclcnt++;
			if (c == '\0' || ep >= dp)
			    goto cerror;
		    }
		} while ((c = *sp++) != ']');
		if (cclcnt > 255)
		    goto cerror;
		lastep[1] = cclcnt;
		continue;

	    case '\\': 
		if ((c = *sp++) == '\0')
		    goto cerror;
		/* FALLTHRU */
	defchar: 
	    default: 
		*ep++ = CCHR;
		*ep++ = c;
	}
    }

cerror: ;
    return 0;
}


static int
GREPaction (params)
plist
{
    int c, body, lf;
    long pos = start;
    char *p1, *p2, *ebp, *cbp;
    char ibuf[BUFSIZ];
    NMH_UNUSED (msgnum);

    fseek (fp, start, SEEK_SET);
    body = 0;
    ebp = cbp = ibuf;
    for (;;) {
	if (body && n->n_header)
	    return 0;
	p1 = linebuf;
	p2 = cbp;
	lf = 0;
	for (;;) {
	    if (p2 >= ebp) {
		if (fgets (ibuf, sizeof ibuf, fp) == NULL
			|| (stop && pos >= stop)) {
		    if (lf)
			break;
		    return 0;
		}
		pos += (long) strlen (ibuf);
		p2 = ibuf;
		ebp = ibuf + strlen (ibuf);
	    }
	    c = *p2++;
	    if (lf && c != '\n') {
		if (c != ' ' && c != '\t') {
		    --p2;
		    break;
		}
                lf = 0;
	    }
	    if (c == '\n') {
		if (body)
		    break;
                if (lf) {
                    body++;
                    break;
                }
                lf++;
                /* Unfold by skipping the newline. */
                c = 0;
	    }
	    if (c && p1 < &linebuf[LBSIZE - 1])
		*p1++ = c;
	}

	*p1++ = 0;
	cbp = p2;
	p1 = linebuf;
	p2 = n->n_expbuf;

	/* Attempt to decode as a MIME header.	If it's the last header,
	   body will be 1 and lf will be at least 1. */
	if ((body == 0 || lf > 0)  &&
	    decode_rfc2047 (linebuf, decoded_linebuf, sizeof decoded_linebuf)) {
	    p1 = decoded_linebuf;
	}

	if (n->n_circf) {
	    if (advance (p1, p2))
		return 1;
	    continue;
	}

	if (*p2 == CCHR) {
	    c = p2[1];
	    do {
		if (*p1 == c || cc[(unsigned char)*p1] == c)
		    if (advance (p1, p2))
			return 1;
	    } while (*p1++);
	    continue;
	}

	do {
	    if (advance (p1, p2))
		return 1;
	} while (*p1++);
    }
}


static int
advance (char *alp, char *aep)
{
    unsigned char *lp, *ep, *curlp;

    lp = (unsigned char *)alp;
    ep = (unsigned char *)aep;
    for (;;)
	switch (*ep++) {
	    case CCHR: 
		if (*ep++ == *lp++ || ep[-1] == cc[lp[-1]])
		    continue;
		return 0;

	    case CDOT: 
		if (*lp++)
		    continue;
		return 0;

	    case CDOL: 
		if (*lp == 0)
		    continue;
		return 0;

	    case CEOF: 
		return 1;

	    case CCL: 
		if (cclass (ep, *lp++, 1)) {
		    ep += *ep + 1;
		    continue;
		}
		return 0;

	    case NCCL: 
		if (cclass (ep, *lp++, 0)) {
		    ep += *ep + 1;
		    continue;
		}
		return 0;

	    case CDOT | STAR: 
		curlp = lp;
		while (*lp++)
		    continue;
		goto star;

	    case CCHR | STAR: 
		curlp = lp;
		while (*lp++ == *ep || cc[lp[-1]] == *ep)
		    continue;
		ep++;
		goto star;

	    case CCL | STAR: 
	    case NCCL | STAR: 
		curlp = lp;
		while (cclass (ep, *lp++, ep[-1] == (CCL | STAR)))
		    continue;
		ep += *ep + 1;
		goto star;

	star: 
		do {
		    lp--;
		    if (advance ((char *) lp, (char *) ep))
			return (1);
		} while (lp > curlp);
		return 0;

	    default: 
		inform("advance() botch -- you lose big, continuing...");
		return 0;
	}
}


static int
cclass (unsigned char *aset, int ac, int af)
{
    unsigned int    n;
    unsigned char   c, *set;

    set = aset;
    if ((c = ac) == 0)
	return (0);

    n = *set++;
    while (n--)
	if (*set++ == c || set[-1] == cc[c])
	    return (af);

    return (!af);
}


static int
tcompile (char *ap, struct tws *tb, int isafter)
{
    struct tws *tw;

    if ((tw = tws_parse (ap, isafter)) == NULL)
	return 0;

    *tb = *tw;
    return 1;
}


static struct tws *
tws_parse (char *ap, int isafter)
{
    char buffer[BUFSIZ];
    struct tws *tw, *ts;

    if ((tw = tws_special (ap)) != NULL) {
	tw->tw_sec = tw->tw_min = isafter ? 59 : 0;
	tw->tw_hour = isafter ? 23 : 0;
	return tw;
    }
    if ((tw = dparsetime (ap)) != NULL)
	return tw;

    if ((ts = dlocaltimenow ()) == NULL)
	return NULL;

    snprintf (buffer, sizeof(buffer), "%s %s", ap, dtwszone (ts));
    if ((tw = dparsetime (buffer)) != NULL)
	return tw;

    snprintf (buffer, sizeof(buffer), "%s %02d:%02d:%02d %s", ap,
	    ts->tw_hour, ts->tw_min, ts->tw_sec, dtwszone (ts));
    if ((tw = dparsetime (buffer)) != NULL)
	return tw;

    snprintf (buffer, sizeof(buffer), "%02d %s %04d %s",
	    ts->tw_mday, tw_moty[ts->tw_mon], ts->tw_year, ap);
    if ((tw = dparsetime (buffer)) != NULL)
	return tw;

    snprintf (buffer, sizeof(buffer), "%02d %s %04d %s %s",
	    ts->tw_mday, tw_moty[ts->tw_mon], ts->tw_year,
	    ap, dtwszone (ts));
    if ((tw = dparsetime (buffer)) != NULL)
	return tw;

    return NULL;
}


static struct tws *
tws_special (char *ap)
{
    int i;
    time_t clock;
    struct tws *tw;

    time (&clock);
    if (!strcasecmp (ap, "today"))
	return dlocaltime (&clock);
    if (!strcasecmp (ap, "yesterday")) {
	clock -= (long) (60 * 60 * 24);
	return dlocaltime (&clock);
    }
    if (!strcasecmp (ap, "tomorrow")) {
	clock += (long) (60 * 60 * 24);
	return dlocaltime (&clock);
    }

    for (i = 0; tw_ldotw[i]; i++)
	if (!strcasecmp (ap, tw_ldotw[i]))
	    break;
    if (tw_ldotw[i]) {
	if ((tw = dlocaltime (&clock)) == NULL)
	    return NULL;
	if ((i -= tw->tw_wday) > 0)
	    i -= 7;
    }
    else {
	if (*ap != '-')
	    return NULL;
	/* -ddd days ago */
	i = atoi (ap);	/* we should error check this */
    }

    clock += (long) ((60 * 60 * 24) * i);
    return dlocaltime (&clock);
}


static int
TWSaction (params)
plist
{
    int state;
    char *bp;
    char buf[NMH_BUFSIZ], name[NAMESZ];
    struct tws *tw;
    m_getfld_state_t gstate = 0;
    NMH_UNUSED (stop);

    fseek (fp, start, SEEK_SET);
    for (bp = NULL;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld (&gstate, name, buf, &bufsz, fp)) {
	    case FLD: 
	    case FLDPLUS: 
                mh_xfree(bp);
		bp = mh_xstrdup(buf);
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld (&gstate, name, buf, &bufsz, fp);
		    bp = add (buf, bp);
		}
		if (!strcasecmp (name, n->n_datef))
		    break;
		continue;

	    case BODY: 
	    case FILEEOF: 
	    case LENERR: 
	    case FMTERR: 
		if (state == LENERR || state == FMTERR)
		    inform("format error in message %d", msgnum);
                mh_xfree(bp);
		return 0;

	    default: 
		adios (NULL, "internal error -- you lose");
	}
	break;
    }
    m_getfld_state_destroy (&gstate);

    if ((tw = dparsetime (bp)) == NULL)
	inform("unable to parse %s field in message %d, matching...",
		n->n_datef, msgnum), state = 1;
    else
	state = n->n_after ? (twsort (tw, &n->n_tws) > 0)
	    : (twsort (tw, &n->n_tws) < 0);

    mh_xfree(bp);
    return state;
}
