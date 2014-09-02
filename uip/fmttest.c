
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
    X("address", 0, ADDRSW) \
    X("raw", 0, RAWSW) \
    X("date", 0, DATESW) \
    X("message", 0, MESSAGESW) \
    X("file", 0, FILESW) \
    X("nofile", 0, NFILESW) \
    X("-component-name component-text", 0, OTHERSW) \
    X("dupaddrs", 0, DUPADDRSW) \
    X("nodupaddrs", 0, NDUPADDRSW) \
    X("ccme", 0, CCMESW) \
    X("noccme", 0, NCCMESW) \
    X("outsize size-in-characters", 0, OUTSIZESW) \
    X("width column-width", 0, WIDTHSW) \
    X("msgnum number", 0, MSGNUMSW) \
    X("msgcur flag", 0, MSGCURSW) \
    X("msgsize size", 0, MSGSIZESW) \
    X("unseen flag", 0, UNSEENSW) \
    X("dump", 0, DUMPSW) \
    X("nodump", 0, NDUMPSW) \
    X("trace", 0, TRACESW) \
    X("notrace", 0, NTRACESW) \
    X("version", 0, VERSIONSW) \
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

enum mode_t { MESSAGE, ADDRESS, DATE, RAW };
#define DEFADDRFORMAT "%<{error}%{error}: %{text}%|%(putstr(proper{text}))%>"
#define DEFDATEFORMAT "%<(nodate{text})error: %{text}%|%(putstr(pretty{text}))%>"

/*
 * Context structure used by the tracing routines
 */

struct trace_context {
    int num;
    char *str;
    char *outbuf;
};

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
static void litputs(const char *);
static void litputc(char);
static void process_addresses(struct format *, struct msgs_array *,
			      charstring_t, int, int *,
			      struct fmt_callbacks *);
static void process_raw(struct format *, struct msgs_array *, charstring_t,
			int, int *, struct fmt_callbacks *);
static void process_messages(struct format *, struct msgs_array *,
			     struct msgs_array *, charstring_t, char *, int,
			     int, int *, struct fmt_callbacks *);
static void process_single_file(FILE *, struct msgs_array *, int *, int,
				struct format *, charstring_t, int,
				struct fmt_callbacks *);
static void test_trace(void *, struct format *, int, char *, const char *);
static char *test_formataddr(char *, char *);
static char *test_concataddr(char *, char *);
static int insert(struct mailname *);
static void mlistfree(void);

static int nodupcheck = 0;	/* If set, no check for duplicates */
static int ccme = 0;		/* Should I cc myself? */
static struct mailname mq;	/* Mail addresses to check for duplicates */
static char *dummy = "dummy";

int
main (int argc, char **argv)
{
    char *cp, *form = NULL, *format = NULL, *defformat = FORMAT, *folder = NULL;
    char buf[BUFSIZ], *nfs, **argp, **arguments;
    charstring_t buffer;
    struct format *fmt;
    struct comp *cptr;
    struct msgs_array msgs = { 0, 0, NULL }, compargs = { 0, 0, NULL};
    int dump = 0, i;
    int outputsize = 0, dupaddrs = 1, trace = 0, files = 0;
    int colwidth = -1, msgnum = -1, msgcur = -1, msgsize = -1, msgunseen = -1;
    enum mode_t mode = MESSAGE;
    int dat[5];
    struct fmt_callbacks cb, *cbp = NULL;

    if (nmh_init(argv[0], 1)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    /*
	     * A -- means that we have a component name (like pick);
	     * save the component name and the next argument for the text.
	     */
	    if (*++cp == '-') {
	    	if (*++cp == '\0')
		    adios(NULL, "missing component name after --");
		app_msgarg(&compargs, cp);
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
			outputsize = INT_MAX;
		    else if (strcmp(cp, "width") == 0)
		    	outputsize = sc_width();
		    else
			outputsize = atoi(cp);
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

		case TRACESW:
		    trace++;
		    continue;
		case NTRACESW:
		    trace = 0;
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
		    dupaddrs = 0;
		    continue;
		case DATESW:
		    mode = DATE;
		    defformat = DEFDATEFORMAT;
		    continue;

		case FILESW:
		    files++;
		    continue;
		case NFILESW:
		    files = 0;
		    continue;

		case DUPADDRSW:
		    dupaddrs++;
		    continue;
		case NDUPADDRSW:
		    dupaddrs = 0;
		    continue;

		case CCMESW:
		    ccme++;
		    continue;
		case NCCMESW:
		    ccme = 0;
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
		case NDUMPSW:
		    dump = 0;
		    continue;

	    }
	}

	/*
	 * Only interpret as a folder if we're in message mode
	 */

	if (mode == MESSAGE && !files && (*cp == '+' || *cp == '@')) {
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
    * If you're picking "raw" as a mode, then you have to select
    * a format.
    */

   if (mode == RAW && form == NULL && format == NULL) {
   	adios (NULL, "You must specify a format with -form or -format when "
	       "using -raw");
   }

    /*
     * Get new format string.  Must be before chdir().
     */
    nfs = new_fs (form, format, defformat);
    (void) fmt_compile(nfs, &fmt, 1);

    if (dump || trace) {
        initlabels(fmt);
	if (dump) {
	    fmt_dump(nfs, fmt);
	    if (compargs.size == 0 && msgs.size == 0)
		done(0);
	}
    }

    buffer = charstring_create(BUFSIZ);

    if (outputsize == 0) {
	if (mode == MESSAGE)
	    outputsize = sc_width();
	else
	    outputsize = INT_MAX;
    }

    dat[0] = msgnum;
    dat[1] = msgcur;
    dat[2] = msgsize;
    dat[3] = colwidth == -1 ? outputsize : colwidth;
    dat[4] = msgunseen;

    /*
     * If we want to provide our own formataddr, concactaddr, or tracing
     * callback, do that now.  Also, prime ismymbox if we use it.
     */

    if (dupaddrs == 0 || trace) {
    	memset(&cb, 0, sizeof(cb));
	cbp = &cb;

	if (dupaddrs == 0) {
	    cb.formataddr = test_formataddr;
	    cb.concataddr = test_concataddr;
	    if (!ccme)
		ismymbox(NULL);
	}

	if (trace) {
	    struct trace_context *ctx;

	    ctx = mh_xmalloc(sizeof(*ctx));
	    ctx->num = -1;
	    ctx->str = dummy;
	    ctx->outbuf = getcpy(NULL);

	    cb.trace_func = test_trace;
	    cb.trace_context = ctx;
	}
    }

    if (mode == MESSAGE) {
	process_messages(fmt, &compargs, &msgs, buffer, folder, outputsize,
			 files, dat, cbp);
    } else {
	if (compargs.size) {
	    for (i = 0; i < compargs.size; i += 2) {
		cptr = fmt_findcomp(compargs.msgs[i]);
		if (cptr)
		    cptr->c_text = getcpy(compargs.msgs[i + 1]);
	    }
	}

	if (mode == ADDRESS) {
	    process_addresses(fmt, &msgs, buffer, outputsize, dat, cbp);
	} else /* Fall-through for RAW or DATE */
	    process_raw(fmt, &msgs, buffer, outputsize, dat, cbp);
    }

    charstring_free(buffer);
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
process_addresses(struct format *fmt, struct msgs_array *addrs,
		  charstring_t buffer, int outwidth, int *dat,
		  struct fmt_callbacks *cb)
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
	    if ((mp = getm(cp, NULL, 0, error, sizeof(error))) == NULL) {
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

	    fmt_scan(fmt, buffer, outwidth, dat, cb);
	    fputs(charstring_buffer(buffer), stdout);
	    mlistfree();

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
 * Process messages and run them through the format engine.  A lot taken
 * from scan.c.
 */

static void
process_messages(struct format *fmt, struct msgs_array *comps,
		 struct msgs_array *msgs, charstring_t buffer, char *folder,
		 int outwidth, int files, int *dat,
		 struct fmt_callbacks *cb)
{
    int i, msgnum, msgsize = dat[2], num = dat[0], cur = dat[1];
    int num_unseen_seq = 0;
    ivector_t seqnum = ivector_create (0);
    char *maildir, *cp;
    struct msgs *mp;
    FILE *in;

    /*
     * If 'files' is set, short-circuit everything else and just process
     * everything now.
     */

    if (files) {
	for (i = 0; i < msgs->size; i++) {
	    if ((in = fopen(cp = msgs->msgs[i], "r")) == NULL) {
		admonish(cp, "unable to open file");
		continue;
	    }
	    process_single_file(in, comps, dat, msgsize, fmt, buffer,
				outwidth, cb);
	}

	return;
    }

    if (! folder)
    	folder = getfolder(1);

    maildir = m_maildir(folder);

    if (chdir(maildir) < 0)
    	adios(maildir, "unable to change directory to");

    if (!(mp = folder_read(folder, 1)))
    	adios(NULL, "unable to read folder %s", folder);

    if (mp->nummsg == 0)
    	adios(NULL, "no messages in %s", folder);

    for (i = 0; i < msgs->size; i++)
    	if (!m_convert(mp, msgs->msgs[i]))
	    done(1);
    seq_setprev(mp);			/* set the Previous-Sequence */

    context_replace(pfolder, folder);	/* update curren folder */
    seq_save(mp);			/* synchronize message sequences */
    context_save();			/* save the context file */

    /*
     * We want to set the unseen flag if requested, so we have to check
     * the unseen sequence as well.
     */

    if (dat[4] == -1) {
    	if ((cp = context_find(usequence)) && *cp) {
	    char **ap, *dp;

	    dp = getcpy(cp);
	    ap = brkstring(dp, " ", "\n");
	    for (i = 0; ap && *ap; i++, ap++)
		ivector_push_back (seqnum, seq_getnum(mp, *ap));
		
	    num_unseen_seq = i;
	    if (dp)
	    	free(dp);
	}
    }

    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
    	if (is_selected(mp, msgnum)) {
	    if ((in = fopen(cp = m_name(msgnum), "r")) == NULL) {
	    	admonish(cp, "unable to open message");
		continue;
	    }

	    fmt_freecomptext();

	    if (num == -1)
	    	dat[0] = msgnum;

	    if (cur == -1)
	    	dat[1] = msgnum == mp->curmsg;

	    /*
	     * Check to see if this is in the unseen sequence
	     */

	    dat[4] = 0;
	    for (i = 0; i < num_unseen_seq; i++) {
		if (in_sequence(mp, ivector_at (seqnum, i), msgnum)) {
		    dat[4] = 1;
		    break;
		}
	    }

	    /*
	     * Read in the message and process the components
	     */

	    process_single_file(in, comps, dat, msgsize, fmt, buffer,
				outwidth, cb);
	}
    }

    ivector_free (seqnum);
    folder_free(mp);
    return;
}

/*
 * Process a single file in message mode
 */

static void
process_single_file(FILE *in, struct msgs_array *comps, int *dat, int msgsize,
		    struct format *fmt, charstring_t buffer, int outwidth,
		    struct fmt_callbacks *cb)
{
    int i, state;
    char name[NAMESZ], rbuf[NMH_BUFSIZ];
    m_getfld_state_t gstate = 0;
    struct comp *c;
    int bufsz;

    /*
     * Get our size if we didn't include one
     */

    if (msgsize == -1) {
	struct stat st;

	if (fstat(fileno(in), &st) < 0)
	    dat[2] = 0;
	else
	    dat[2] = st.st_size;
    }

    /*
     * Initialize everyting else
     */

    if (dat[0] == -1)
    	dat[0] = 0;
    if (dat[1] == -1)
    	dat[1] = 0;
    if (dat[4] == -1)
    	dat[4] = 0;

    /*
     * Read in the message and process the components
     */

    for (state = FLD;;) {
	bufsz = sizeof(rbuf);
	state = m_getfld(&gstate, name, rbuf, &bufsz, in);
	switch (state) {
	case FLD:
	case FLDPLUS:
	    i = fmt_addcomptext(name, rbuf);
	    if (i != -1) {
		while (state == FLDPLUS) {
		    bufsz = sizeof(rbuf);
		    state = m_getfld(&gstate, name, rbuf, &bufsz, in);
		    fmt_appendcomp(i, name, rbuf);
		}
	    }

	    while (state == FLDPLUS) {
		bufsz = sizeof(rbuf);
		state = m_getfld(&gstate, name, rbuf, &bufsz, in);
	    }
	    break;

	case BODY:
	    if (fmt_findcomp("body")) {
		if ((i = strlen(rbuf)) < outwidth) {
		    bufsz = min (outwidth, (int) sizeof rbuf - i);
		    state = m_getfld(&gstate, name, rbuf + i,
				     &bufsz, in);
		}

		fmt_addcomptext("body", rbuf);
	    }
	    /* fall through */

	default:
	    goto finished;
	}
    }
finished:
    fclose(in);
    m_getfld_state_destroy(&gstate);

    /*
     * Do this now to override any components in the original message
     */
    if (comps->size) {
	for (i = 0; i < comps->size; i += 2) {
	    c = fmt_findcomp(comps->msgs[i]);
	    if (c) {
		if (c->c_text)
		    free(c->c_text);
		c->c_text = getcpy(comps->msgs[i + 1]);
	    }
	}
    }
    fmt_scan(fmt, buffer, outwidth, dat, cb);
    fputs(charstring_buffer (buffer), stdout);
    mlistfree();
}

/*
 * Run text through the format engine with no special processing
 */

static void
process_raw(struct format *fmt, struct msgs_array *text, charstring_t buffer,
	    int outwidth, int *dat, struct fmt_callbacks *cb)
{
    int i;
    struct comp *c;

    if (dat[0] == -1)
    	dat[0] = 0;
    if (dat[1] == -1)
    	dat[1] = 0;
    if (dat[2] == -1)
    	dat[2] = 0;
    if (dat[4] == -1)
    	dat[4] = 0;

    c = fmt_findcomp("text");

    for (i = 0; i < text->size; i++) {
    	if (c != NULL) {
	    if (c->c_text != NULL)
	    	free(c->c_text);
	    c->c_text = getcpy(text->msgs[i]);
	}

	fmt_scan(fmt, buffer, outwidth, dat, cb);
	fputs(charstring_buffer (buffer), stdout);
	mlistfree();
    }
}

/*
 * Our basic tracing support callback.
 *
 * Print out each instruction as it's executed, including the values of
 * the num and str registers if they've changed.
 */

static void
test_trace(void *context, struct format *fmt, int num, char *str,
	   const char *outbuf)
{
    struct trace_context *ctx = (struct trace_context *) context;
    int changed = 0;

    dumpone(fmt);

    if (num != ctx->num) {
    	printf("num=%d", num);
	ctx->num = num;
	changed++;
    }

    if (str != ctx->str) {
    	if (changed++)
	    printf(" ");
	printf("str=");
	litputs(str);
	ctx->str = str;
    }

    if (changed)
    	printf("\n");

    if (strcmp(outbuf, ctx->outbuf) != 0) {
    	printf("outbuf=");
	litputs(outbuf);
	putchar('\n');
    	free(ctx->outbuf);
	ctx->outbuf = getcpy(outbuf);
    }
}

static void
fmt_dump (char *nfs, struct format *fmth)
{
	struct format *fmt;

	printf("Instruction dump of format string: \n%s\n", nfs);

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
	case FT_LS_DECODECOMP: return("LS_DECODECOMP");
	case FT_LS_DECODE: return("LS_DECODE");
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
	case FT_LS_UNQUOTE: return("LS_UNQUOTE");
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
		snprintf(buf, sizeof(buf), "/* ??? #%d */", t);
		return(buf);
	}
}

static char *
c_typestr(int t)
{
	static char buf[64];

	snprintb(buf, sizeof(buf), t, CT_BITS);
	return(buf);
}

static char *
c_flagsstr(int t)
{
	static char buf[64];

	snprintb(buf, sizeof(buf), t, CF_BITS);
	return(buf);
}

static void
litputs(const char *s)
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
		printf("\\x%02x", (unsigned char) c);
	} else if (c < 0x20 || c == 0177) {
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

/*
 * Routines/code to support the duplicate address suppression code, adapted
 * from replsbr.c
 */

static char *buf;		/* our current working buffer */
static char *bufend;		/* end of working buffer */
static char *last_dst;		/* buf ptr at end of last call */
static unsigned int bufsiz=0;	/* current size of buf */

#define BUFINCR 512		/* how much to expand buf when if fills */

#define CPY(s) { cp = (s); while ((*dst++ = *cp++)) ; --dst; }

/*
 * check if there's enough room in buf for str.
 * add more mem if needed
 */
#define CHECKMEM(str) \
	    if ((len = strlen (str)) >= bufend - dst) {\
		int i = dst - buf;\
		int n = last_dst - buf;\
		bufsiz += ((dst + len - bufend) / BUFINCR + 1) * BUFINCR;\
		buf = mh_xrealloc (buf, bufsiz);\
		dst = buf + i;\
		last_dst = buf + n;\
		bufend = buf + bufsiz;\
	    }


/*
 * These are versions of similar routines from replsbr.c; the purpose is
 * to suppress duplicate addresses from being added to a list when building
 * up addresses for the %(formataddr) format function.  This is used by
 * repl to prevent duplicate addresses from being added to the "to" line.
 * See replsbr.c for more information.
 *
 * We can't use the functions in replsbr.c directly because they are slightly
 * different and depend on the rest of replsbr.c
 */
static char *
test_formataddr (char *orig, char *str)
{
    register int len;
    char error[BUFSIZ];
    register int isgroup;
    register char *dst;
    register char *cp;
    register char *sp;
    register struct mailname *mp = NULL;

    /* if we don't have a buffer yet, get one */
    if (bufsiz == 0) {
	buf = mh_xmalloc (BUFINCR);
	last_dst = buf;		/* XXX */
	bufsiz = BUFINCR - 6;  /* leave some slop */
	bufend = buf + bufsiz;
    }
    /*
     * If "orig" points to our buffer we can just pick up where we
     * left off.  Otherwise we have to copy orig into our buffer.
     */
    if (orig == buf)
	dst = last_dst;
    else if (!orig || !*orig) {
	dst = buf;
	*dst = '\0';
    } else {
	dst = last_dst;		/* XXX */
	CHECKMEM (orig);
	CPY (orig);
    }

    /* concatenate all the new addresses onto 'buf' */
    for (isgroup = 0; (cp = getname (str)); ) {
	if ((mp = getm (cp, NULL, 0, error, sizeof(error))) == NULL) {
	    fprintf(stderr, "bad address \"%s\" -- %s\n", cp, error);
	    continue;
	}
	if (isgroup && (mp->m_gname || !mp->m_ingrp)) {
	    *dst++ = ';';
	    isgroup = 0;
	}
	if (insert (mp)) {
	    /* if we get here we're going to add an address */
	    if (dst != buf) {
		*dst++ = ',';
		*dst++ = ' ';
	    }
	    if (mp->m_gname) {
		CHECKMEM (mp->m_gname);
		CPY (mp->m_gname);
		isgroup++;
	    }
	    sp = adrformat (mp);
	    CHECKMEM (sp);
	    CPY (sp);
	}
    }

    if (isgroup)
	*dst++ = ';';

    *dst = '\0';
    last_dst = dst;
    return (buf);
}


/*
 * The companion to test_formataddr(); it behaves the same way, except doesn't
 * do duplicate address detection.
 */
static char *
test_concataddr(char *orig, char *str)
{
    char *cp;

    nodupcheck = 1;
    cp = test_formataddr(orig, str);
    nodupcheck = 0;
    return cp;
}

static int
insert (struct mailname *np)
{
    struct mailname *mp;

    if (nodupcheck)
	return 1;

    if (np->m_mbox == NULL)
	return 0;

    for (mp = &mq; mp->m_next; mp = mp->m_next) {
	if (!strcasecmp (np->m_host ? np->m_host : "",
			 mp->m_next->m_host ? mp->m_next->m_host : "") &&
	    !strcasecmp (np->m_mbox ? np->m_mbox : "",
			 mp->m_next->m_mbox ? mp->m_next->m_mbox : ""))
	    return 0;
    }
    if (!ccme && ismymbox (np))
	return 0;

    mp->m_next = np;

    return 1;
}

/*
 * Reset our duplicate address list
 */

void
mlistfree(void)
{
    struct mailname *mp, *mp2;

    for (mp = mq.m_next; mp; mp = mp2->m_next) {
    	mp2 = mp;
	mnfree(mp);
    }
}
