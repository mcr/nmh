
/*
 * fmttest.c -- A program to help test and debug format instructions
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/fmt_scan.h>
#include <h/fmt_compile.h>
#include <h/utils.h>
#include <h/scansbr.h>
#include <h/addrsbr.h>

#define FMTTEST_SWITCHES \
    X("form formatfile", 0, FORMSW) \
    X("format string", 5, FMTSW) \
    X("dump", 0, DUMPSW) \
    X("address", 0, ADDRSW) \
    X("raw", 0, RAWSW) \
    X("date", 0, DATESW) \
    X("message", 0, MESSAGESW) \
    X("normalize", 0, NORMSW) \
    X("nonormalize", 0, NNORMSW) \
    X("outsize size-in-characters", 0, OUTSIZESW) \
    X("bufsize size-in-bytes", 0, BUFSZSW) \
    X("width column-width", 0, WIDTHSW) \
    X("msgnum number", 0, MSGNUMSW) \
    X("msgcur flag", 0, MSGCURSW) \
    X("msgsize size", 0, MSGSIZESW) \
    X("unseen flag", 0, UNSEENSW) \
    X("version", 0, VERSIONSW) \
    X("-component-name component-text", 0, OTHERSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(FMTTEST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(FMTTEST, switches);
#undef X

/*
 * An array containing labels used for branch instructions
 */

static struct format **lvec = NULL;
static int lused = 0;
static int lallocated = 0;

enum mode_t { MESSAGE, ADDRESS, RAW };
#define DEFADDRFORMAT "%<{error}%{error}: %{text}%|%(putstr(proper{text}))%>"
#define DEFDATEFORMAT "%<(nodate{text})error: %{text}%|%(putstr(pretty{text}))%>"

/*
 * static prototypes
 */
static void fmt_dump (char *, struct format *);
static void dumpone(struct format *);
static void initlabels(struct format *);
static int findlabel(struct format *);
static void assignlabel(struct format *);
static char *f_typestr(int);
static char *c_typestr(int);
static char *c_flagsstr(int);
static void litputs(char *);
static void litputc(char);
static void process_addresses(struct format *, struct msgs_array *, char *,
			      int, int, int *, int);
static void process_raw(struct format *, struct msgs_array *, char *,
			int, int, int *);
static void process_messages(struct format *, struct msgs_array *,
			     struct msgs_array *, char *, int, int, int *);


int
main (int argc, char **argv)
{
    char *cp, *form = NULL, *format = NULL, *defformat = FORMAT, *folder = NULL;
    char buf[BUFSIZ], *nfs, **argp, **arguments, *buffer;
    struct format *fmt;
    struct comp *cptr;
    struct msgs_array msgs = { 0, 0, NULL }, compargs = { 0, 0, NULL};
    int dump = 0, i;
    int outputsize = 0, bufsize = 0;
    int colwidth = -1, msgnum = -1, msgcur = -1, msgsize = -1, msgunseen = -1;
    int normalize = AD_HOST;
    enum mode_t mode = MESSAGE;
    int dat[5];

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
	    /*
	     * A -- means that we have a component name (like pick);
	     * save the component name and the next argument for the text.
	     */
	    if (*++cp == '-') {
		app_msgarg(&compargs, --cp);
		/* Grab next argument for component text */
		if (!(cp = *argp++))
		    adios(NULL, "missing argument to %s", argp[-2]);
		app_msgarg(&compargs, cp);
		continue;
	    }
	    switch (smatch (cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches]", invo_name);
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);
		case OTHERSW:
		    adios(NULL, "internal argument error!");
		    continue;

		case OUTSIZESW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios(NULL, "missing argument to %s", argp[-2]);
		    if (strcmp(cp, "max") == 0)
		    	outputsize = -1;
		    else if (strcmp(cp, "width") == 0)
		    	outputsize = sc_width();
		    else
			outputsize = atoi(cp);
		    continue;
		case BUFSZSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios(NULL, "missing argument to %s", argp[-2]);
		    bufsize = atoi(cp);
		    continue;

		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    format = NULL;
		    continue;
		case FMTSW: 
		    if (!(format = *argp++) || *format == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    form = NULL;
		    continue;

		case NORMSW:
		    normalize = AD_HOST;
		    continue;
		case NNORMSW:
		    normalize = AD_NHST;
		    continue;

		case ADDRSW:
		    mode = ADDRESS;
		    defformat = DEFADDRFORMAT;
		    continue;
		case RAWSW:
		    mode = RAW;
		    continue;
		case MESSAGESW:
		    mode = MESSAGE;
		    defformat = FORMAT;
		    continue;
		case DATESW:
		    mode = RAW;
		    defformat = DEFDATEFORMAT;
		    continue;

		case WIDTHSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios(NULL, "missing argument to %s", argp[-2]);
		    colwidth = atoi(cp);
		    continue;
		case MSGNUMSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios(NULL, "missing argument to %s", argp[-2]);
		    msgnum = atoi(cp);
		    continue;
		case MSGCURSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios(NULL, "missing argument to %s", argp[-2]);
		    msgcur = atoi(cp);
		    continue;
		case MSGSIZESW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios(NULL, "missing argument to %s", argp[-2]);
		    msgsize = atoi(cp);
		    continue;
		case UNSEENSW:
		    if (!(cp = *argp++) || *cp == '-')
		    	adios(NULL, "missing argument to %s", argp[-2]);
		    msgunseen = atoi(cp);
		    continue;

		case DUMPSW:
		    dump++;
		    continue;

	    }
	}

	/*
	 * Only interpret as a folder if we're in message mode
	 */

	if (mode == MESSAGE && (*cp == '+' || *cp == '@')) {
	    if (folder)
	    	adios (NULL, "only one folder at a time!");
	    else
	    	folder = pluspath (cp);
	} else
	    app_msgarg(&msgs, cp);
    }

    /*
     * Here's our weird heuristic:
     *
     * - We allow -dump without any other arguments.
     * - If you've given any component arguments, we don't require any
     *   other arguments.
     * - The arguments are interpreted as folders/messages _if_ we're in
     *   message mode, otherwise pass as strings in the text component.
     */

   if (!dump && compargs.size == 0 && msgs.size == 0) {
   	adios (NULL, "usage: [switches] [+folder] msgs | strings...",
	       invo_name);
   }

    /*
     * Get new format string.  Must be before chdir().
     */
    nfs = new_fs (form, format, defformat);
    (void) fmt_compile(nfs, &fmt, 1);

    if (dump) {
	fmt_dump(nfs, fmt);
	if (compargs.size == 0 && msgs.size == 0)
	    done(0);
    }

    /*
     * If we don't specify a buffer size, allocate a default one.
     */

    if (bufsize == 0)
    	bufsize = BUFSIZ;

    buffer = mh_xmalloc(bufsize);

    if (outputsize < 0)
    	outputsize = bufsize - 1;	/* For the trailing NUL */
    else if (outputsize == 0) {
    	if (mode == ADDRESS) 
	    outputsize = sc_width();
	else
	    outputsize = bufsize - 1;
    }

    dat[0] = msgnum;
    dat[1] = msgcur;
    dat[2] = msgsize;
    dat[3] = colwidth == -1 ? outputsize : colwidth;
    dat[4] = msgunseen;

    if (mode == MESSAGE) {
    	process_messages(fmt, &compargs, &msgs, buffer, bufsize, outputsize,
			 dat);
    } else {
	if (compargs.size) {
	    for (i = 0; i < compargs.size; i += 2) {
		cptr = fmt_findcomp(compargs.msgs[i]);
		if (cptr)
		    cptr->c_text = getcpy(compargs.msgs[i + 1]);
	    }
	}

	if (mode == ADDRESS) {
    	    fmt_norm = normalize;
	    process_addresses(fmt, &msgs, buffer, bufsize, outputsize, dat,
	    		      normalize);
	} else
	    process_raw(fmt, &msgs, buffer, bufsize, outputsize, dat);
    }

    fmt_free(fmt, 1);

    done(0);
    return 1;
}

/*
 * Process each address with fmt_scan().
 */

struct pqpair {
    char *pq_text;
    char *pq_error;
    struct pqpair *pq_next;
};

static void
process_addresses(struct format *fmt, struct msgs_array *addrs, char *buffer,
		  int bufsize, int outwidth, int *dat, int norm)
{
    int i;
    char *cp, error[BUFSIZ];
    struct mailname *mp;
    struct pqpair *p, *q;
    struct pqpair pq;
    struct comp *c;

    if (dat[0] == -1)
    	dat[0] = 0;
    if (dat[1] == -1)
    	dat[1] = 0;
    if (dat[2] == -1)
    	dat[2] = 0;
    if (dat[4] == -1)
    	dat[4] = 0;

    for (i = 0; i < addrs->size; i++) {
    	(q = &pq)->pq_next = NULL;
	while ((cp = getname(addrs->msgs[i]))) {
	    if ((p = (struct pqpair *) calloc ((size_t) 1, sizeof(*p))) == NULL)
	    	adios (NULL, "unable to allocate pqpair memory");
	    if ((mp = getm(cp, NULL, 0, norm, error)) == NULL) {
	    	p->pq_text = getcpy(cp);
		p->pq_error = getcpy(error);
	    } else {
	    	p->pq_text = getcpy(mp->m_text);
		mnfree(mp);
	    }
	    q = (q->pq_next = p);
	}

	for (p = pq.pq_next; p; p = q) {
	    c = fmt_findcomp("text");
	    if (c) {
	    	if (c->c_text)
		    free(c->c_text);
		c->c_text = p->pq_text;
		p->pq_text = NULL;
	    }
	    c = fmt_findcomp("error");
	    if (c) {
	    	if (c->c_text)
		    free(c->c_text);
		c->c_text = p->pq_error;
		p->pq_error = NULL;
	    }

	    fmt_scan(fmt, buffer, bufsize, outwidth, dat);
	    fputs(buffer, stdout);

	    if (p->pq_text)
	    	free(p->pq_text);
	    if (p->pq_error)
	    	free(p->pq_error);
	    q = p->pq_next;
	    free(p);
	}
    }
}

/*
 * Process messages and run them through the format engine
 */

static void
process_messages(struct format *fmt, struct msgs_array *comps,
		 struct msgs_array *msgs, char *buffer, int bufsize,
		 int outwidth, int *dat)
{
}

/*
 * Run text through the format engine with no special processing
 */

static void
process_raw(struct format *fmt, struct msgs_array *text, char *buffer,
	    int bufsize, int outwidth, int *dat)
{
}

static void
fmt_dump (char *nfs, struct format *fmth)
{
	struct format *fmt;

	printf("Instruction dump of format string: \n%s\n", nfs);

	initlabels(fmth);

	/* Dump them out! */
	for (fmt = fmth; fmt; ++fmt) {
		dumpone(fmt);
		if (fmt->f_type == FT_DONE && fmt->f_value == 0)
			break;
	}
}

static void
dumpone(struct format *fmt)
{
	register int i;

	if ((i = findlabel(fmt)) >= 0)
		printf("L%d:", i);
	putchar('\t');

	fputs(f_typestr((int)fmt->f_type), stdout);

	switch (fmt->f_type) {

	case FT_COMP:
	case FT_LS_COMP:
	case FT_LV_COMPFLAG:
	case FT_LV_COMP:
		printf(", comp ");
		litputs(fmt->f_comp->c_name);
		if (fmt->f_comp->c_type)
			printf(", c_type %s", c_typestr(fmt->f_comp->c_type));
		if (fmt->f_comp->c_flags)
			printf(", c_flags %s", c_flagsstr(fmt->f_comp->c_flags));
		break;

	case FT_LV_SEC:
	case FT_LV_MIN:
	case FT_LV_HOUR:
	case FT_LV_MDAY:
	case FT_LV_MON:
	case FT_LS_MONTH:
	case FT_LS_LMONTH:
	case FT_LS_ZONE:
	case FT_LV_YEAR:
	case FT_LV_WDAY:
	case FT_LS_DAY:
	case FT_LS_WEEKDAY:
	case FT_LV_YDAY:
	case FT_LV_ZONE:
	case FT_LV_CLOCK:
	case FT_LV_RCLOCK:
	case FT_LV_DAYF:
	case FT_LV_ZONEF:
	case FT_LV_DST:
	case FT_LS_822DATE:
	case FT_LS_PRETTY:
	case FT_LOCALDATE:
	case FT_GMTDATE:
	case FT_PARSEDATE:
		printf(", c_name ");
		litputs(fmt->f_comp->c_name);
		if (fmt->f_comp->c_type)
			printf(", c_type %s", c_typestr(fmt->f_comp->c_type));
		if (fmt->f_comp->c_flags)
			printf(", c_flags %s", c_flagsstr(fmt->f_comp->c_flags));
		break;

	case FT_LS_ADDR:
	case FT_LS_PERS:
	case FT_LS_MBOX:
	case FT_LS_HOST:
	case FT_LS_PATH:
	case FT_LS_GNAME:
	case FT_LS_NOTE:
	case FT_LS_822ADDR:
	case FT_LV_HOSTTYPE:
	case FT_LV_INGRPF:
	case FT_LV_NOHOSTF:
	case FT_LS_FRIENDLY:
	case FT_PARSEADDR:
	case FT_MYMBOX:
		printf(", c_name ");
		litputs(fmt->f_comp->c_name);
		if (fmt->f_comp->c_type)
			printf(", c_type %s", c_typestr(fmt->f_comp->c_type));
		if (fmt->f_comp->c_flags)
			printf(", c_flags %s", c_flagsstr(fmt->f_comp->c_flags));
		break;

	case FT_COMPF:
		printf(", width %d, fill '", fmt->f_width);
		litputc(fmt->f_fill);
		printf("' name ");
		litputs(fmt->f_comp->c_name);
		if (fmt->f_comp->c_type)
			printf(", c_type %s", c_typestr(fmt->f_comp->c_type));
		if (fmt->f_comp->c_flags)
			printf(", c_flags %s", c_flagsstr(fmt->f_comp->c_flags));
		break;

	case FT_STRF:
	case FT_NUMF:
		printf(", width %d, fill '", fmt->f_width);
		litputc(fmt->f_fill);
		putchar('\'');
		break;

	case FT_LIT:
#ifdef FT_LIT_FORCE
	case FT_LIT_FORCE:
#endif
		putchar(' ');
		litputs(fmt->f_text);
		break;

	case FT_LITF:
		printf(", width %d, fill '", fmt->f_width);
		litputc(fmt->f_fill);
		printf("' ");
		litputs(fmt->f_text);
		break;

	case FT_CHAR:
		putchar(' ');
		putchar('\'');
		litputc(fmt->f_char);
		putchar('\'');
		break;


	case FT_IF_S:
	case FT_IF_S_NULL:
	case FT_IF_MATCH:
	case FT_IF_AMATCH:
		printf(" continue else goto");
	case FT_GOTO:
		i = findlabel(fmt + fmt->f_skip);
		printf(" L%d", i);
		break;

	case FT_IF_V_EQ:
	case FT_IF_V_NE:
	case FT_IF_V_GT:
		i = findlabel(fmt + fmt->f_skip);
		printf(" %d continue else goto L%d", fmt->f_value, i);
		break;

	case FT_V_EQ:
	case FT_V_NE:
	case FT_V_GT:
	case FT_LV_LIT:
	case FT_LV_PLUS_L:
	case FT_LV_MINUS_L:
	case FT_LV_DIVIDE_L:
	case FT_LV_MODULO_L:
		printf(" value %d", fmt->f_value);
		break;

	case FT_LS_LIT:
		printf(" str ");
		litputs(fmt->f_text);
		break;

	case FT_LS_GETENV:
		printf(" getenv ");
		litputs(fmt->f_text);
		break;

	case FT_LS_DECODECOMP:
		printf(", comp ");
		litputs(fmt->f_comp->c_name);
		break;

	case FT_LS_DECODE:
		break;

	case FT_LS_TRIM:
		printf(", width %d", fmt->f_width);
		break;

	case FT_LV_DAT:
		printf(", value dat[%d]", fmt->f_value);
		break;
	}
	putchar('\n');
}

/*
 * Iterate over all instructions and assign labels to the targets of
 * branch statements
 */

static void
initlabels(struct format *fmth)
{
	struct format *fmt, *addr;
	int i;

	/* Assign labels */
	for (fmt = fmth; fmt; ++fmt) {
		i = fmt->f_type;
		if (i == FT_IF_S ||
		    i == FT_IF_S_NULL ||
		    i == FT_IF_V_EQ ||
		    i == FT_IF_V_NE ||
		    i == FT_IF_V_GT ||
		    i == FT_IF_MATCH ||
		    i == FT_IF_AMATCH ||
		    i == FT_GOTO) {
			addr = fmt + fmt->f_skip;
			if (findlabel(addr) < 0)
				assignlabel(addr);
		}
		if (fmt->f_type == FT_DONE && fmt->f_value == 0)
			break;
	}
}


static int
findlabel(struct format *addr)
{
	register int i;

	for (i = 0; i < lused; ++i)
		if (addr == lvec[i])
			return(i);
	return(-1);
}

static void
assignlabel(struct format *addr)
{
	if (lused >= lallocated) {
		lallocated += 64;
		lvec = (struct format **)
			mh_xrealloc(lvec, sizeof(struct format *) * lallocated);
	}
	
	lvec[lused++] = addr;
}

static char *
f_typestr(int t)
{
	static char buf[32];

	switch (t) {
	case FT_COMP: return("COMP");
	case FT_COMPF: return("COMPF");
	case FT_LIT: return("LIT");
	case FT_LITF: return("LITF");
#ifdef	FT_LIT_FORCE
	case FT_LIT_FORCE: return("LIT_FORCE");
#endif
	case FT_CHAR: return("CHAR");
	case FT_NUM: return("NUM");
	case FT_NUMF: return("NUMF");
	case FT_STR: return("STR");
	case FT_STRF: return("STRF");
	case FT_STRFW: return("STRFW");
	case FT_PUTADDR: return("PUTADDR");
	case FT_STRLIT: return("STRLIT");
	case FT_STRLITZ: return("STRLITZ");
	case FT_LS_COMP: return("LS_COMP");
	case FT_LS_LIT: return("LS_LIT");
	case FT_LS_GETENV: return("LS_GETENV");
	case FT_LS_DECODECOMP: return("FT_LS_DECODECOMP");
	case FT_LS_DECODE: return("FT_LS_DECODE");
	case FT_LS_TRIM: return("LS_TRIM");
	case FT_LV_COMP: return("LV_COMP");
	case FT_LV_COMPFLAG: return("LV_COMPFLAG");
	case FT_LV_LIT: return("LV_LIT");
	case FT_LV_DAT: return("LV_DAT");
	case FT_LV_STRLEN: return("LV_STRLEN");
	case FT_LV_PLUS_L: return("LV_PLUS_L");
	case FT_LV_MINUS_L: return("LV_MINUS_L");
	case FT_LV_DIVIDE_L: return("LV_DIVIDE_L");
	case FT_LV_MODULO_L: return("LV_MODULO_L");
	case FT_LV_CHAR_LEFT: return("LV_CHAR_LEFT");
	case FT_LS_MONTH: return("LS_MONTH");
	case FT_LS_LMONTH: return("LS_LMONTH");
	case FT_LS_ZONE: return("LS_ZONE");
	case FT_LS_DAY: return("LS_DAY");
	case FT_LS_WEEKDAY: return("LS_WEEKDAY");
	case FT_LS_822DATE: return("LS_822DATE");
	case FT_LS_PRETTY: return("LS_PRETTY");
	case FT_LV_SEC: return("LV_SEC");
	case FT_LV_MIN: return("LV_MIN");
	case FT_LV_HOUR: return("LV_HOUR");
	case FT_LV_MDAY: return("LV_MDAY");
	case FT_LV_MON: return("LV_MON");
	case FT_LV_YEAR: return("LV_YEAR");
	case FT_LV_YDAY: return("LV_YDAY");
	case FT_LV_WDAY: return("LV_WDAY");
	case FT_LV_ZONE: return("LV_ZONE");
	case FT_LV_CLOCK: return("LV_CLOCK");
	case FT_LV_RCLOCK: return("LV_RCLOCK");
	case FT_LV_DAYF: return("LV_DAYF");
	case FT_LV_DST: return("LV_DST");
	case FT_LV_ZONEF: return("LV_ZONEF");
	case FT_LS_ADDR: return("LS_ADDR");
	case FT_LS_PERS: return("LS_PERS");
	case FT_LS_MBOX: return("LS_MBOX");
	case FT_LS_HOST: return("LS_HOST");
	case FT_LS_PATH: return("LS_PATH");
	case FT_LS_GNAME: return("LS_GNAME");
	case FT_LS_NOTE: return("LS_NOTE");
	case FT_LS_822ADDR: return("LS_822ADDR");
	case FT_LS_FRIENDLY: return("LS_FRIENDLY");
	case FT_LV_HOSTTYPE: return("LV_HOSTTYPE");
	case FT_LV_INGRPF: return("LV_INGRPF");
	case FT_LV_NOHOSTF: return("LV_NOHOSTF");
	case FT_LOCALDATE: return("LOCALDATE");
	case FT_GMTDATE: return("GMTDATE");
	case FT_PARSEDATE: return("PARSEDATE");
	case FT_PARSEADDR: return("PARSEADDR");
	case FT_FORMATADDR: return("FORMATADDR");
	case FT_CONCATADDR: return("CONCATADDR");
	case FT_MYMBOX: return("MYMBOX");
#ifdef	FT_ADDTOSEQ
	case FT_ADDTOSEQ: return("ADDTOSEQ");
#endif
	case FT_SAVESTR: return("SAVESTR");
#ifdef	FT_PAUSE
	case FT_PAUSE: return ("PAUSE");
#endif
	case FT_DONE: return("DONE");
	case FT_NOP: return("NOP");
	case FT_GOTO: return("GOTO");
	case FT_IF_S_NULL: return("IF_S_NULL");
	case FT_IF_S: return("IF_S");
	case FT_IF_V_EQ: return("IF_V_EQ");
	case FT_IF_V_NE: return("IF_V_NE");
	case FT_IF_V_GT: return("IF_V_GT");
	case FT_IF_MATCH: return("IF_MATCH");
	case FT_IF_AMATCH: return("IF_AMATCH");
	case FT_S_NULL: return("S_NULL");
	case FT_S_NONNULL: return("S_NONNULL");
	case FT_V_EQ: return("V_EQ");
	case FT_V_NE: return("V_NE");
	case FT_V_GT: return("V_GT");
	case FT_V_MATCH: return("V_MATCH");
	case FT_V_AMATCH: return("V_AMATCH");
	default:
		printf(buf, "/* ??? #%d */", t);
		return(buf);
	}
}

#define FNORD(v, s) if (t & (v)) { \
	if (i++ > 0) \
		strcat(buf, "|"); \
	strcat(buf, s); }

static char *
c_typestr(int t)
{
	register int i;
	static char buf[64];

	buf[0] = '\0';
	if (t & ~(CT_ADDR|CT_DATE))
		printf(buf, "0x%x ", t);
	strcat(buf, "<");
	i = 0;
	FNORD(CT_ADDR, "ADDR");
	FNORD(CT_DATE, "DATE");
	strcat(buf, ">");
	return(buf);
}

static char *
c_flagsstr(int t)
{
	register int i;
	static char buf[64];

	buf[0] = '\0';
	if (t & ~(CF_TRUE|CF_PARSED|CF_DATEFAB|CF_TRIMMED))
		printf(buf, "0x%x ", t);
	strcat(buf, "<");
	i = 0;
	FNORD(CF_TRUE, "TRUE");
	FNORD(CF_PARSED, "PARSED");
	FNORD(CF_DATEFAB, "DATEFAB");
	FNORD(CF_TRIMMED, "TRIMMED");
	strcat(buf, ">");
	return(buf);
}
#undef FNORD

static void
litputs(char *s)
{
	if (s) {
		putc('"', stdout);
		while (*s)
			litputc(*s++);
		putc('"', stdout);
	} else
		fputs("<nil>", stdout);
}

static void
litputc(char c)
{
	if (c & ~ 0177) {
		putc('M', stdout);
		putc('-', stdout);
		c &= 0177;
	}
	if (c < 0x20 || c == 0177) {
		if (c == '\b') {
			putc('\\', stdout);
			putc('b', stdout);
		} else if (c == '\f') {
			putc('\\', stdout);
			putc('f', stdout);
		} else if (c == '\n') {
			putc('\\', stdout);
			putc('n', stdout);
		} else if (c == '\r') {
			putc('\\', stdout);
			putc('r', stdout);
		} else if (c == '\t') {
			putc('\\', stdout);
			putc('t', stdout);
		} else {
			putc('^', stdout);
			putc(c ^ 0x40, stdout);	/* DEL to ?, others to alpha */
		}
	} else
		putc(c, stdout);
}
