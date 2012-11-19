
/*
 * mhbuildsbr.c -- routines to expand/translate MIME composition files
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 * This code was originally part of mhn.c.  I split it into
 * a separate program (mhbuild.c) and then later split it
 * again (mhbuildsbr.c).  But the code still has some of
 * the mhn.c code in it.  This program needs additional
 * streamlining and removal of unneeded code.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <errno.h>
#include <signal.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/utils.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>


extern int debugsw;
extern int verbosw;

extern int ebcdicsw;
extern int listsw;
extern int rfc934sw;
extern int contentidsw;

/* cache policies */
extern int rcachesw;	/* mhcachesbr.c */
extern int wcachesw;	/* mhcachesbr.c */

/*
 * Directory to place tmp files.  This must
 * be set before these routines are called.
 */
char *tmp;

pid_t xpid = 0;

static char prefix[] = "----- =_aaaaaaaaaa";


/* mhmisc.c */
void content_error (char *, CT, char *, ...);

/* mhcachesbr.c */
int find_cache (CT, int, int *, char *, char *, int);

/* mhfree.c */
void free_content (CT);
void free_ctinfo (CT);
void free_encoding (CT, int);

/*
 * prototypes
 */
CT build_mime (char *, int);

/*
 * static prototypes
 */
static int init_decoded_content (CT);
static char *fgetstr (char *, int, FILE *);
static int user_content (FILE *, char *, char *, CT *);
static void set_id (CT, int);
static int compose_content (CT);
static int scan_content (CT);
static int build_headers (CT);
static char *calculate_digest (CT, int);


static unsigned char directives_stack[32];
static unsigned int directives_index;

static int do_direct(void)
{
    return directives_stack[directives_index];
}

static void directive_onoff(int onoff)
{
    if (directives_index >= sizeof(directives_stack) - 1) {
	fprintf(stderr, "mhbuild: #on/off overflow, continuing\n");
	return;
    }
    directives_stack[++directives_index] = onoff;
}

static void directive_init(int onoff)
{
    directives_index = 0;
    directives_stack[0] = onoff;
}

static void directive_pop(void)
{
    if (directives_index > 0)
	directives_index--;
    else
	fprintf(stderr, "mhbuild: #pop underflow, continuing\n");
}

/*
 * Main routine for translating composition file
 * into valid MIME message.  It translates the draft
 * into a content structure (actually a tree of content
 * structures).  This message then can be manipulated
 * in various ways, including being output via
 * output_message().
 */

CT
build_mime (char *infile, int directives)
{
    int	compnum, state;
    char buf[BUFSIZ], name[NAMESZ];
    char *cp, *np, *vp;
    struct multipart *m;
    struct part **pp;
    CT ct;
    FILE *in;

    directive_init(directives);

    umask (~m_gmprot ());

    /* open the composition draft */
    if ((in = fopen (infile, "r")) == NULL)
	adios (infile, "unable to open for reading");

    /*
     * Allocate space for primary (outside) content
     */
    if ((ct = (CT) calloc (1, sizeof(*ct))) == NULL)
	adios (NULL, "out of memory");

    /*
     * Allocate structure for handling decoded content
     * for this part.  We don't really need this, but
     * allocate it to remain consistent.
     */
    init_decoded_content (ct);

    /*
     * Parse some of the header fields in the composition
     * draft into the linked list of header fields for
     * the new MIME message.
     */
    for (compnum = 1, state = FLD;;) {
	switch (state = m_getfld (state, name, buf, sizeof(buf), in)) {
	case FLD:
	case FLDPLUS:
	case FLDEOF:
	    compnum++;

	    /* abort if draft has Mime-Version header field */
	    if (!mh_strcasecmp (name, VRSN_FIELD))
		adios (NULL, "draft shouldn't contain %s: field", VRSN_FIELD);

	    /* abort if draft has Content-Transfer-Encoding header field */
	    if (!mh_strcasecmp (name, ENCODING_FIELD))
		adios (NULL, "draft shouldn't contain %s: field", ENCODING_FIELD);

	    /* ignore any Content-Type fields in the header */
	    if (!mh_strcasecmp (name, TYPE_FIELD)) {
		while (state == FLDPLUS)
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		goto finish_field;
	    }

	    /* get copies of the buffers */
	    np = add (name, NULL);
	    vp = add (buf, NULL);

	    /* if necessary, get rest of field */
	    while (state == FLDPLUS) {
		state = m_getfld (state, name, buf, sizeof(buf), in);
		vp = add (buf, vp);	/* add to previous value */
	    }

	    /* Now add the header data to the list */
	    add_header (ct, np, vp);

finish_field:
	    /* if this wasn't the last header field, then continue */
	    if (state != FLDEOF)
		continue;
	    /* else fall... */

	case FILEEOF:
	    adios (NULL, "draft has empty body -- no directives!");
	    /* NOTREACHED */

	case BODY:
	case BODYEOF:
	    fseek (in, (long) (-strlen (buf)), SEEK_CUR);
	    break;

	case LENERR:
	case FMTERR:
	    adios (NULL, "message format error in component #%d", compnum);

	default:
	    adios (NULL, "getfld() returned %d", state);
	}
	break;
    }

    /*
     * Now add the MIME-Version header field
     * to the list of header fields.
     */
    np = add (VRSN_FIELD, NULL);
    vp = concat (" ", VRSN_VALUE, "\n", NULL);
    add_header (ct, np, vp);

    /*
     * We initally assume we will find multiple contents in the
     * draft.  So create a multipart/mixed content to hold everything.
     * We can remove this later, if it is not needed.
     */
    if (get_ctinfo ("multipart/mixed", ct, 0) == NOTOK)
	done (1);
    ct->c_type = CT_MULTIPART;
    ct->c_subtype = MULTI_MIXED;
    ct->c_file = add (infile, NULL);

    if ((m = (struct multipart *) calloc (1, sizeof(*m))) == NULL)
	adios (NULL, "out of memory");
    ct->c_ctparams = (void *) m;
    pp = &m->mp_parts;

    /*
     * read and parse the composition file
     * and the directives it contains.
     */
    while (fgetstr (buf, sizeof(buf) - 1, in)) {
	struct part *part;
	CT p;

	if (user_content (in, infile, buf, &p) == DONE) {
	    admonish (NULL, "ignoring spurious #end");
	    continue;
	}
	if (!p)
	    continue;

	if ((part = (struct part *) calloc (1, sizeof(*part))) == NULL)
	    adios (NULL, "out of memory");
	*pp = part;
	pp = &part->mp_next;
	part->mp_part = p;
    }

    /*
     * close the composition draft since
     * it's not needed any longer.
     */
    fclose (in);

    /* check if any contents were found */
    if (!m->mp_parts)
	adios (NULL, "no content directives found");

    /*
     * If only one content was found, then remove and
     * free the outer multipart content.
     */
    if (!m->mp_parts->mp_next) {
	CT p;

	p = m->mp_parts->mp_part;
	m->mp_parts->mp_part = NULL;

	/* move header fields */
	p->c_first_hf = ct->c_first_hf;
	p->c_last_hf = ct->c_last_hf;
	ct->c_first_hf = NULL;
	ct->c_last_hf = NULL;

	free_content (ct);
	ct = p;
    } else {
	set_id (ct, 1);
    }

    /*
     * Fill out, or expand directives.  Parse and execute
     * commands specified by profile composition strings.
     */
    compose_content (ct);

    if ((cp = strchr(prefix, 'a')) == NULL)
	adios (NULL, "internal error(4)");

    /*
     * Scan the contents.  Choose a transfer encoding, and
     * check if prefix for multipart boundary clashes with
     * any of the contents.
     */
    while (scan_content (ct) == NOTOK) {
	if (*cp < 'z') {
	    (*cp)++;
        } else {
	    if (*++cp == 0)
		adios (NULL, "giving up trying to find a unique delimiter string");
    	    else
		(*cp)++;
	}
    }

    /* Build the rest of the header field structures */
    build_headers (ct);

    return ct;
}


/*
 * Set up structures for placing unencoded
 * content when building parts.
 */

static int
init_decoded_content (CT ct)
{
    CE ce;

    if ((ce = (CE) calloc (1, sizeof(*ce))) == NULL)
	adios (NULL, "out of memory");

    ct->c_cefile     = ce;
    ct->c_ceopenfnx  = open7Bit;	/* since unencoded */
    ct->c_ceclosefnx = close_encoding;
    ct->c_cesizefnx  = NULL;		/* since unencoded */

    return OK;
}


static char *
fgetstr (char *s, int n, FILE *stream)
{
    char *cp, *ep;
    int o_n = n;

    while(1) {
	for (ep = (cp = s) + o_n; cp < ep; ) {
	    int i;

	    if (!fgets (cp, n, stream))
		return (cp != s ? s : NULL);

	    if (cp == s && *cp != '#')
		return s;

	    cp += (i = strlen (cp)) - 1;
	    if (i <= 1 || *cp-- != '\n' || *cp != '\\')
		break;
	    *cp = '\0';
	    n -= (i - 2);
	}

	if (strcmp(s, "#on\n") == 0) {
	    directive_onoff(1);
	} else if (strcmp(s, "#off\n") == 0) {
	    directive_onoff(0);
	} else if (strcmp(s, "#pop\n") == 0) {
	    directive_pop();
	} else {
	    break;
	}
    }

    return s;
}


/*
 * Parse the composition draft for text and directives.
 * Do initial setup of Content structure.
 */

static int
user_content (FILE *in, char *file, char *buf, CT *ctp)
{
    int	extrnal, vrsn;
    unsigned char *cp;
    char **ap;
    char buffer[BUFSIZ];
    struct multipart *m;
    struct part **pp;
    struct stat st;
    struct str2init *s2i;
    CI ci;
    CT ct;
    CE ce;

    if (buf[0] == '\n' || (do_direct() && strcmp (buf, "#\n") == 0)) {
	*ctp = NULL;
	return OK;
    }

    /* allocate basic Content structure */
    if ((ct = (CT) calloc (1, sizeof(*ct))) == NULL)
	adios (NULL, "out of memory");
    *ctp = ct;

    /* allocate basic structure for handling decoded content */
    init_decoded_content (ct);
    ce = ct->c_cefile;

    ci = &ct->c_ctinfo;
    set_id (ct, 0);

    /*
     * Handle inline text.  Check if line
     * is one of the following forms:
     *
     * 1) doesn't begin with '#'	(implicit directive)
     * 2) begins with "##"		(implicit directive)
     * 3) begins with "#<"
     */
    if (!do_direct() || buf[0] != '#' || buf[1] == '#' || buf[1] == '<') {
	int headers;
	int inlineD;
	long pos;
	char content[BUFSIZ];
	FILE *out;
        char *cp;

        cp = m_mktemp2(NULL, invo_name, NULL, &out);
        if (cp == NULL) adios("mhbuildsbr", "unable to create temporary file");

	/* use a temp file to collect the plain text lines */
	ce->ce_file = add (cp, NULL);
	ce->ce_unlink = 1;

	if (do_direct() && (buf[0] == '#' && buf[1] == '<')) {
	    strncpy (content, buf + 2, sizeof(content));
	    inlineD = 1;
	    goto rock_and_roll;
	} else {
	    inlineD = 0;
	}

	/* the directive is implicit */
	strncpy (content, "text/plain", sizeof(content));
	headers = 0;
	strncpy (buffer, (!do_direct() || buf[0] != '#') ? buf : buf + 1, sizeof(buffer));
	for (;;) {
	    int	i;

	    if (headers >= 0 && do_direct() && uprf (buffer, DESCR_FIELD)
		&& buffer[i = strlen (DESCR_FIELD)] == ':') {
		headers = 1;

again_descr:
		ct->c_descr = add (buffer + i + 1, ct->c_descr);
		if (!fgetstr (buffer, sizeof(buffer) - 1, in))
		    adios (NULL, "end-of-file after %s: field in plaintext", DESCR_FIELD);
		switch (buffer[0]) {
		case ' ':
		case '\t':
		    i = -1;
		    goto again_descr;

		case '#':
		    adios (NULL, "#-directive after %s: field in plaintext", DESCR_FIELD);
		    /* NOTREACHED */

		default:
		    break;
		}
	    }

	    if (headers >= 0 && do_direct() && uprf (buffer, DISPO_FIELD)
		&& buffer[i = strlen (DISPO_FIELD)] == ':') {
		headers = 1;

again_dispo:
		ct->c_dispo = add (buffer + i + 1, ct->c_dispo);
		if (!fgetstr (buffer, sizeof(buffer) - 1, in))
		    adios (NULL, "end-of-file after %s: field in plaintext", DISPO_FIELD);
		switch (buffer[0]) {
		case ' ':
		case '\t':
		    i = -1;
		    goto again_dispo;

		case '#':
		    adios (NULL, "#-directive after %s: field in plaintext", DISPO_FIELD);
		    /* NOTREACHED */

		default:
		    break;
		}
	    }

	    if (headers != 1 || buffer[0] != '\n')
		fputs (buffer, out);

rock_and_roll:
	    headers = -1;
	    pos = ftell (in);
	    if ((cp = fgetstr (buffer, sizeof(buffer) - 1, in)) == NULL)
		break;
	    if (do_direct() && buffer[0] == '#') {
		char *bp;

		if (buffer[1] != '#')
		    break;
		for (cp = (bp = buffer) + 1; *cp; cp++)
		    *bp++ = *cp;
		*bp = '\0';
	    }
	}

	if (listsw)
	    ct->c_end = ftell (out);
	fclose (out);

	/* parse content type */
	if (get_ctinfo (content, ct, inlineD) == NOTOK)
	    done (1);

	for (s2i = str2cts; s2i->si_key; s2i++)
	    if (!mh_strcasecmp (ci->ci_type, s2i->si_key))
		break;
	if (!s2i->si_key && !uprf (ci->ci_type, "X-"))
	    s2i++;

	/*
	 * check type specified (possibly implicitly)
	 */
	switch (ct->c_type = s2i->si_val) {
	case CT_MESSAGE:
	    if (!mh_strcasecmp (ci->ci_subtype, "rfc822")) {
		ct->c_encoding = CE_7BIT;
		goto call_init;
	    }
	    /* else fall... */
	case CT_MULTIPART:
	    adios (NULL, "it doesn't make sense to define an in-line %s content",
		   ct->c_type == CT_MESSAGE ? "message" : "multipart");
	    /* NOTREACHED */

	default:
call_init:
	    if ((ct->c_ctinitfnx = s2i->si_init))
		(*ct->c_ctinitfnx) (ct);
	    break;
	}

	if (cp)
	    fseek (in, pos, SEEK_SET);
	return OK;
    }

    /*
     * If we've reached this point, the next line
     * must be some type of explicit directive.
     */

    /* check if directive is external-type */
    extrnal = (buf[1] == '@');

    /* parse directive */
    if (get_ctinfo (buf + (extrnal ? 2 : 1), ct, 1) == NOTOK)
	done (1);

    /* check directive against the list of MIME types */
    for (s2i = str2cts; s2i->si_key; s2i++)
	if (!mh_strcasecmp (ci->ci_type, s2i->si_key))
	    break;

    /*
     * Check if the directive specified a valid type.
     * This will happen if it was one of the following forms:
     *
     *    #type/subtype  (or)
     *    #@type/subtype
     */
    if (s2i->si_key) {
	if (!ci->ci_subtype)
	    adios (NULL, "missing subtype in \"#%s\"", ci->ci_type);

	switch (ct->c_type = s2i->si_val) {
	case CT_MULTIPART:
	    adios (NULL, "use \"#begin ... #end\" instead of \"#%s/%s\"",
		   ci->ci_type, ci->ci_subtype);
	    /* NOTREACHED */

	case CT_MESSAGE:
	    if (!mh_strcasecmp (ci->ci_subtype, "partial"))
		adios (NULL, "sorry, \"#%s/%s\" isn't supported",
		       ci->ci_type, ci->ci_subtype);
	    if (!mh_strcasecmp (ci->ci_subtype, "external-body"))
		adios (NULL, "use \"#@type/subtype ... [] ...\" instead of \"#%s/%s\"",
		       ci->ci_type, ci->ci_subtype);
use_forw:
	    adios (NULL,
		   "use \"#forw [+folder] [msgs]\" instead of \"#%s/%s\"",
		   ci->ci_type, ci->ci_subtype);
	    /* NOTREACHED */

	default:
	    if ((ct->c_ctinitfnx = s2i->si_init))
		(*ct->c_ctinitfnx) (ct);
	    break;
	}

	/*
	 * #@type/subtype (external types directive)
	 */
	if (extrnal) {
	    struct exbody *e;
	    CT p;

	    if (!ci->ci_magic)
		adios (NULL, "need external information for \"#@%s/%s\"",
		       ci->ci_type, ci->ci_subtype);
	    p = ct;

	    snprintf (buffer, sizeof(buffer), "message/external-body; %s", ci->ci_magic);
	    free (ci->ci_magic);
	    ci->ci_magic = NULL;

	    /*
	     * Since we are using the current Content structure to
	     * hold information about the type of the external
	     * reference, we need to create another Content structure
	     * for the message/external-body to wrap it in.
	     */
	    if ((ct = (CT) calloc (1, sizeof(*ct))) == NULL)
		adios (NULL, "out of memory");
	    *ctp = ct;
	    ci = &ct->c_ctinfo;
	    if (get_ctinfo (buffer, ct, 0) == NOTOK)
		done (1);
	    ct->c_type = CT_MESSAGE;
	    ct->c_subtype = MESSAGE_EXTERNAL;

	    if ((e = (struct exbody *) calloc (1, sizeof(*e))) == NULL)
		adios (NULL, "out of memory");
	    ct->c_ctparams = (void *) e;

	    e->eb_parent = ct;
	    e->eb_content = p;
	    p->c_ctexbody = e;

	    if (params_external (ct, 1) == NOTOK)
		done (1);

	    return OK;
	}

	/* Handle [file] argument */
	if (ci->ci_magic) {
	    /* check if specifies command to execute */
	    if (*ci->ci_magic == '|' || *ci->ci_magic == '!') {
		for (cp = ci->ci_magic + 1; isspace (*cp); cp++)
		    continue;
		if (!*cp)
		    adios (NULL, "empty pipe command for #%s directive", ci->ci_type);
		cp = add (cp, NULL);
		free (ci->ci_magic);
		ci->ci_magic = cp;
	    } else {
		/* record filename of decoded contents */
		ce->ce_file = ci->ci_magic;
		if (access (ce->ce_file, R_OK) == NOTOK)
		    adios ("reading", "unable to access %s for", ce->ce_file);
		if (listsw && stat (ce->ce_file, &st) != NOTOK)
		    ct->c_end = (long) st.st_size;
		ci->ci_magic = NULL;
	    }
	    return OK;
	}

	/*
	 * No [file] argument, so check profile for
	 * method to compose content.
	 */
	snprintf (buffer, sizeof(buffer), "%s-compose-%s/%s",
		invo_name, ci->ci_type, ci->ci_subtype);
	if ((cp = context_find (buffer)) == NULL || *cp == '\0') {
	    snprintf (buffer, sizeof(buffer), "%s-compose-%s", invo_name, ci->ci_type);
	    if ((cp = context_find (buffer)) == NULL || *cp == '\0') {
		content_error (NULL, ct, "don't know how to compose content");
		done (1);
	    }
	}
	ci->ci_magic = add (cp, NULL);
	return OK;
    }

    if (extrnal)
	adios (NULL, "external definition not allowed for \"#%s\"", ci->ci_type);

    /*
     * Message directive
     * #forw [+folder] [msgs]
     */
    if (!mh_strcasecmp (ci->ci_type, "forw")) {
	int msgnum;
	char *folder, *arguments[MAXARGS];
	struct msgs *mp;

	if (ci->ci_magic) {
	    ap = brkstring (ci->ci_magic, " ", "\n");
	    copyip (ap, arguments, MAXARGS);
	} else {
	    arguments[0] = "cur";
	    arguments[1] = NULL;
	}
	folder = NULL;

	/* search the arguments for a folder name */
	for (ap = arguments; *ap; ap++) {
	    cp = *ap;
	    if (*cp == '+' || *cp == '@') {
		if (folder)
		    adios (NULL, "only one folder per #forw directive");
		else
		    folder = pluspath (cp);
	    }
	}

	/* else, use the current folder */
	if (!folder)
	    folder = add (getfolder (1), NULL);

	if (!(mp = folder_read (folder)))
	    adios (NULL, "unable to read folder %s", folder);
	for (ap = arguments; *ap; ap++) {
	    cp = *ap;
	    if (*cp != '+' && *cp != '@')
		if (!m_convert (mp, cp))
		    done (1);
	}
	free (folder);
	free_ctinfo (ct);

	/*
	 * If there is more than one message to include, make this
	 * a content of type "multipart/digest" and insert each message
	 * as a subpart.  If there is only one message, then make this
	 * a content of type "message/rfc822".
	 */
	if (mp->numsel > 1) {
	    /* we are forwarding multiple messages */
	    if (get_ctinfo ("multipart/digest", ct, 0) == NOTOK)
		done (1);
	    ct->c_type = CT_MULTIPART;
	    ct->c_subtype = MULTI_DIGEST;

	    if ((m = (struct multipart *) calloc (1, sizeof(*m))) == NULL)
		adios (NULL, "out of memory");
	    ct->c_ctparams = (void *) m;
	    pp = &m->mp_parts;

	    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
		if (is_selected(mp, msgnum)) {
		    struct part *part;
		    CT p;
		    CE pe;

		    if ((p = (CT) calloc (1, sizeof(*p))) == NULL)
			adios (NULL, "out of memory");
		    init_decoded_content (p);
		    pe = p->c_cefile;
		    if (get_ctinfo ("message/rfc822", p, 0) == NOTOK)
			done (1);
		    p->c_type = CT_MESSAGE;
		    p->c_subtype = MESSAGE_RFC822;

		    snprintf (buffer, sizeof(buffer), "%s/%d", mp->foldpath, msgnum);
		    pe->ce_file = add (buffer, NULL);
		    if (listsw && stat (pe->ce_file, &st) != NOTOK)
			p->c_end = (long) st.st_size;

		    if ((part = (struct part *) calloc (1, sizeof(*part))) == NULL)
			adios (NULL, "out of memory");
		    *pp = part;
		    pp = &part->mp_next;
		    part->mp_part = p;
		}
	    }
	} else {
	    /* we are forwarding one message */
	    if (get_ctinfo ("message/rfc822", ct, 0) == NOTOK)
		done (1);
	    ct->c_type = CT_MESSAGE;
	    ct->c_subtype = MESSAGE_RFC822;

	    msgnum = mp->lowsel;
	    snprintf (buffer, sizeof(buffer), "%s/%d", mp->foldpath, msgnum);
	    ce->ce_file = add (buffer, NULL);
	    if (listsw && stat (ce->ce_file, &st) != NOTOK)
		ct->c_end = (long) st.st_size;
	}

	folder_free (mp);	/* free folder/message structure */
	return OK;
    }

    /*
     * #end
     */
    if (!mh_strcasecmp (ci->ci_type, "end")) {
	free_content (ct);
	*ctp = NULL;
	return DONE;
    }

    /*
     * #begin [ alternative | parallel ]
     */
    if (!mh_strcasecmp (ci->ci_type, "begin")) {
	if (!ci->ci_magic) {
	    vrsn = MULTI_MIXED;
	    cp = SubMultiPart[vrsn - 1].kv_key;
	} else if (!mh_strcasecmp (ci->ci_magic, "alternative")) {
	    vrsn = MULTI_ALTERNATE;
	    cp = SubMultiPart[vrsn - 1].kv_key;
	} else if (!mh_strcasecmp (ci->ci_magic, "parallel")) {
	    vrsn = MULTI_PARALLEL;
	    cp = SubMultiPart[vrsn - 1].kv_key;
	} else if (uprf (ci->ci_magic, "digest")) {
	    goto use_forw;
	} else {
	    vrsn = MULTI_UNKNOWN;
	    cp = ci->ci_magic;
	}

	free_ctinfo (ct);
	snprintf (buffer, sizeof(buffer), "multipart/%s", cp);
	if (get_ctinfo (buffer, ct, 0) == NOTOK)
	    done (1);
	ct->c_type = CT_MULTIPART;
	ct->c_subtype = vrsn;

	if ((m = (struct multipart *) calloc (1, sizeof(*m))) == NULL)
	    adios (NULL, "out of memory");
	ct->c_ctparams = (void *) m;

	pp = &m->mp_parts;
	while (fgetstr (buffer, sizeof(buffer) - 1, in)) {
	    struct part *part;
	    CT p;

	    if (user_content (in, file, buffer, &p) == DONE) {
		if (!m->mp_parts)
		    adios (NULL, "empty \"#begin ... #end\" sequence");
		return OK;
	    }
	    if (!p)
		continue;

	    if ((part = (struct part *) calloc (1, sizeof(*part))) == NULL)
		adios (NULL, "out of memory");
	    *pp = part;
	    pp = &part->mp_next;
	    part->mp_part = p;
	}
	admonish (NULL, "premature end-of-file, missing #end");
	return OK;
    }

    /*
     * Unknown directive
     */
    adios (NULL, "unknown directive \"#%s\"", ci->ci_type);
    return NOTOK;	/* NOT REACHED */
}


static void
set_id (CT ct, int top)
{
    char contentid[BUFSIZ];
    static int partno;
    static time_t clock = 0;
    static char *msgfmt;

    if (clock == 0) {
	time (&clock);
	snprintf (contentid, sizeof(contentid), "%s\n", message_id (clock, 1));
	partno = 0;
	msgfmt = getcpy(contentid);
    }
    snprintf (contentid, sizeof(contentid), msgfmt, top ? 0 : ++partno);
    ct->c_id = getcpy (contentid);
}


static char ebcdicsafe[0x100] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


/*
 * Fill out, or expand the various contents in the composition
 * draft.  Read-in any necessary files.  Parse and execute any
 * commands specified by profile composition strings.
 */

static int
compose_content (CT ct)
{
    CE ce = ct->c_cefile;

    switch (ct->c_type) {
    case CT_MULTIPART:
    {
	int partnum;
	char *pp;
	char partnam[BUFSIZ];
	struct multipart *m = (struct multipart *) ct->c_ctparams;
	struct part *part;

	if (ct->c_partno) {
	    snprintf (partnam, sizeof(partnam), "%s.", ct->c_partno);
	    pp = partnam + strlen (partnam);
	} else {
	    pp = partnam;
	}

	/* first, we call compose_content on all the subparts */
	for (part = m->mp_parts, partnum = 1; part; part = part->mp_next, partnum++) {
	    CT p = part->mp_part;

	    sprintf (pp, "%d", partnum);
	    p->c_partno = add (partnam, NULL);
	    if (compose_content (p) == NOTOK)
		return NOTOK;
	}

	/*
	 * If the -rfc934mode switch is given, then check all
	 * the subparts of a multipart/digest.  If they are all
	 * message/rfc822, then mark this content and all
	 * subparts with the rfc934 compatibility mode flag.
	 */
	if (rfc934sw && ct->c_subtype == MULTI_DIGEST) {
	    int	is934 = 1;

	    for (part = m->mp_parts; part; part = part->mp_next) {
		CT p = part->mp_part;

		if (p->c_subtype != MESSAGE_RFC822) {
		    is934 = 0;
		    break;
		}
	    }
	    ct->c_rfc934 = is934;
	    for (part = m->mp_parts; part; part = part->mp_next) {
		CT p = part->mp_part;

		if ((p->c_rfc934 = is934))
		    p->c_end++;
	    }
	}

	if (listsw) {
	    ct->c_end = (partnum = strlen (prefix) + 2) + 2;
	    if (ct->c_rfc934)
		ct->c_end += 1;

	    for (part = m->mp_parts; part; part = part->mp_next)
		ct->c_end += part->mp_part->c_end + partnum;
	}
    }
    break;

    case CT_MESSAGE:
	/* Nothing to do for type message */
	break;

    /*
     * Discrete types (text/application/audio/image/video)
     */
    default:
	if (!ce->ce_file) {
	    pid_t child_id;
	    int i, xstdout, len, buflen;
	    char *bp, **ap, *cp;
	    char *vec[4], buffer[BUFSIZ];
	    FILE *out;
	    CI ci = &ct->c_ctinfo;
            char *tfile = NULL;

	    if (!(cp = ci->ci_magic))
		adios (NULL, "internal error(5)");

            tfile = m_mktemp2(NULL, invo_name, NULL, NULL);
            if (tfile == NULL) {
                adios("mhbuildsbr", "unable to create temporary file");
            }
	    ce->ce_file = add (tfile, NULL);
	    ce->ce_unlink = 1;

	    xstdout = 0;

	    /* Get buffer ready to go */
	    bp = buffer;
	    bp[0] = '\0';
	    buflen = sizeof(buffer);

	    /*
	     * Parse composition string into buffer
	     */
	    for ( ; *cp; cp++) {
		if (*cp == '%') {
		    switch (*++cp) {
		    case 'a':
		    {
			/* insert parameters from directive */
			char **ep;
			char *s = "";

			for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
			    snprintf (bp, buflen, "%s%s=\"%s\"", s, *ap, *ep);
			    len = strlen (bp);
			    bp += len;
			    buflen -= len;
			    s = " ";
			}
		    }
		    break;

		    case 'F':
			/* %f, and stdout is not-redirected */
			xstdout = 1;
			/* and fall... */

		    case 'f':
			/*
			 * insert temporary filename where
			 * content should be written
			 */
			snprintf (bp, buflen, "%s", ce->ce_file);
			break;

		    case 's':
			/* insert content subtype */
			strncpy (bp, ci->ci_subtype, buflen);
			break;

		    case '%':
			/* insert character % */
			goto raw;

		    default:
			*bp++ = *--cp;
			*bp = '\0';
			buflen--;
			continue;
		    }
		    len = strlen (bp);
		    bp += len;
		    buflen -= len;
		} else {
raw:
		*bp++ = *cp;
		*bp = '\0';
		buflen--;
		}
	    }

	    if (verbosw)
		printf ("composing content %s/%s from command\n\t%s\n",
			ci->ci_type, ci->ci_subtype, buffer);

	    fflush (stdout);	/* not sure if need for -noverbose */

	    vec[0] = "/bin/sh";
	    vec[1] = "-c";
	    vec[2] = buffer;
	    vec[3] = NULL;

	    if ((out = fopen (ce->ce_file, "w")) == NULL)
		adios (ce->ce_file, "unable to open for writing");

	    for (i = 0; (child_id = vfork()) == NOTOK && i > 5; i++)
		sleep (5);
	    switch (child_id) {
	    case NOTOK:
		adios ("fork", "unable to fork");
		/* NOTREACHED */

	    case OK:
		if (!xstdout)
		    dup2 (fileno (out), 1);
		close (fileno (out));
		execvp ("/bin/sh", vec);
		fprintf (stderr, "unable to exec ");
		perror ("/bin/sh");
		_exit (-1);
		/* NOTREACHED */

	    default:
		fclose (out);
		if (pidXwait(child_id, NULL))
		    done (1);
		break;
	    }
	}

	/* Check size of file */
	if (listsw && ct->c_end == 0L) {
	    struct stat st;

	    if (stat (ce->ce_file, &st) != NOTOK)
		ct->c_end = (long) st.st_size;
	}
	break;
    }

    return OK;
}


/*
 * Scan the content.
 *
 *    1) choose a transfer encoding.
 *    2) check for clashes with multipart boundary string.
 *    3) for text content, figure out which character set is being used.
 *
 * If there is a clash with one of the contents and the multipart boundary,
 * this function will exit with NOTOK.  This will cause the scanning process
 * to be repeated with a different multipart boundary.  It is possible
 * (although highly unlikely) that this scan will be repeated multiple times.
 */

static int
scan_content (CT ct)
{
    int len;
    int check8bit = 0, contains8bit = 0;  /* check if contains 8bit data                */
    int checklinelen = 0, linelen = 0;	  /* check for long lines                       */
    int checkboundary = 0, boundaryclash = 0; /* check if clashes with multipart boundary   */
    int checklinespace = 0, linespace = 0;  /* check if any line ends with space          */
    int checkebcdic = 0, ebcdicunsafe = 0;  /* check if contains ebcdic unsafe characters */
    unsigned char *cp = NULL, buffer[BUFSIZ];
    struct text *t = NULL;
    FILE *in = NULL;
    CE ce = ct->c_cefile;

    /*
     * handle multipart by scanning all subparts
     * and then checking their encoding.
     */
    if (ct->c_type == CT_MULTIPART) {
	struct multipart *m = (struct multipart *) ct->c_ctparams;
	struct part *part;

	/* initially mark the domain of enclosing multipart as 7bit */
	ct->c_encoding = CE_7BIT;

	for (part = m->mp_parts; part; part = part->mp_next) {
	    CT p = part->mp_part;

	    if (scan_content (p) == NOTOK)	/* choose encoding for subpart */
		return NOTOK;

	    /* if necessary, enlarge encoding for enclosing multipart */
	    if (p->c_encoding == CE_BINARY)
		ct->c_encoding = CE_BINARY;
	    if (p->c_encoding == CE_8BIT && ct->c_encoding != CE_BINARY)
		ct->c_encoding = CE_8BIT;
	}

	return OK;
    }

    /*
     * Decide what to check while scanning this content.
     */
    switch (ct->c_type) {
    case CT_TEXT:
	check8bit = 1;
	checkboundary = 1;
	if (ct->c_subtype == TEXT_PLAIN) {
	    checkebcdic = 0;
	    checklinelen = 0;
	    checklinespace = 0;
	} else {
	    checkebcdic = ebcdicsw;
	    checklinelen = 1;
	    checklinespace = 1;
	}
	break;

    case CT_APPLICATION:
	check8bit = 1;
	checkebcdic = ebcdicsw;
	checklinelen = 1;
	checklinespace = 1;
	checkboundary = 1;
	break;

    case CT_MESSAGE:
	check8bit = 0;
	checkebcdic = 0;
	checklinelen = 0;
	checklinespace = 0;

	/* don't check anything for message/external */
	if (ct->c_subtype == MESSAGE_EXTERNAL)
	    checkboundary = 0;
	else
	    checkboundary = 1;
	break;

    case CT_AUDIO:
    case CT_IMAGE:
    case CT_VIDEO:
	/*
	 * Don't check anything for these types,
	 * since we are forcing use of base64.
	 */
	check8bit = 0;
	checkebcdic = 0;
	checklinelen = 0;
	checklinespace = 0;
	checkboundary = 0;
	break;
    }

    /*
     * Scan the unencoded content
     */
    if (check8bit || checklinelen || checklinespace || checkboundary) {
	if ((in = fopen (ce->ce_file, "r")) == NULL)
	    adios (ce->ce_file, "unable to open for reading");
	len = strlen (prefix);

	while (fgets (buffer, sizeof(buffer) - 1, in)) {
	    /*
	     * Check for 8bit data.
	     */
	    if (check8bit) {
		for (cp = buffer; *cp; cp++) {
		    if (!isascii (*cp)) {
			contains8bit = 1;
			check8bit = 0;	/* no need to keep checking */
		    }
		    /*
		     * Check if character is ebcdic-safe.  We only check
		     * this if also checking for 8bit data.
		     */
		    if (checkebcdic && !ebcdicsafe[*cp & 0xff]) {
			ebcdicunsafe = 1;
			checkebcdic = 0; /* no need to keep checking */
		    }
		}
	    }

	    /*
	     * Check line length.
	     */
	    if (checklinelen && (strlen (buffer) > CPERLIN + 1)) {
		linelen = 1;
		checklinelen = 0;	/* no need to keep checking */
	    }

	    /*
	     * Check if line ends with a space.
	     */
	    if (checklinespace && (cp = buffer + strlen (buffer) - 2) > buffer && isspace (*cp)) {
		linespace = 1;
		checklinespace = 0;	/* no need to keep checking */
	    }

	    /*
	     * Check if content contains a line that clashes
	     * with our standard boundary for multipart messages.
	     */
	    if (checkboundary && buffer[0] == '-' && buffer[1] == '-') {
		for (cp = buffer + strlen (buffer) - 1; cp >= buffer; cp--)
		    if (!isspace (*cp))
			break;
		*++cp = '\0';
		if (!strncmp(buffer + 2, prefix, len) && isdigit(buffer[2 + len])) {
		    boundaryclash = 1;
		    checkboundary = 0;	/* no need to keep checking */
		}
	    }
	}
	fclose (in);
    }

    /*
     * Decide which transfer encoding to use.
     */
    switch (ct->c_type) {
    case CT_TEXT:
	/*
	 * If the text content didn't specify a character
	 * set, we need to figure out which one was used.
	 */
	t = (struct text *) ct->c_ctparams;
	if (t->tx_charset == CHARSET_UNSPECIFIED) {
	    CI ci = &ct->c_ctinfo;
	    char **ap, **ep;

	    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++)
		continue;

	    if (contains8bit) {
		*ap = concat ("charset=", write_charset_8bit(), NULL);
	    } else {
		*ap = add ("charset=us-ascii", NULL);
	    }
	    t->tx_charset = CHARSET_SPECIFIED;

	    cp = strchr(*ap++, '=');
	    *ap = NULL;
	    *cp++ = '\0';
	    *ep = cp;
	}

	if (contains8bit || ebcdicunsafe || linelen || linespace || checksw)
	    ct->c_encoding = CE_QUOTED;
	else
	    ct->c_encoding = CE_7BIT;
	break;

    case CT_APPLICATION:
	/* For application type, use base64, except when postscript */
	if (contains8bit || ebcdicunsafe || linelen || linespace || checksw)
	    ct->c_encoding = (ct->c_subtype == APPLICATION_POSTSCRIPT)
		? CE_QUOTED : CE_BASE64;
	else
	    ct->c_encoding = CE_7BIT;
	break;

    case CT_MESSAGE:
	ct->c_encoding = CE_7BIT;
	break;

    case CT_AUDIO:
    case CT_IMAGE:
    case CT_VIDEO:
	/* For audio, image, and video contents, just use base64 */
	ct->c_encoding = CE_BASE64;
	break;
    }

    return (boundaryclash ? NOTOK : OK);
}


/*
 * Scan the content structures, and build header
 * fields that will need to be output into the
 * message.
 */

static int
build_headers (CT ct)
{
    int	cc, mailbody, len;
    char **ap, **ep;
    char *np, *vp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /*
     * If message is type multipart, then add the multipart
     * boundary to the list of attribute/value pairs.
     */
    if (ct->c_type == CT_MULTIPART) {
	char *cp;
	static int level = 0;	/* store nesting level */

	ap = ci->ci_attrs;
	ep = ci->ci_values;
	snprintf (buffer, sizeof(buffer), "boundary=%s%d", prefix, level++);
	cp = strchr(*ap++ = add (buffer, NULL), '=');
	*ap = NULL;
	*cp++ = '\0';
	*ep = cp;
    }

    /*
     * Skip the output of Content-Type, parameters, content
     * description and disposition, and Content-ID if the
     * content is of type "message" and the rfc934 compatibility
     * flag is set (which means we are inside multipart/digest
     * and the switch -rfc934mode was given).
     */
    if (ct->c_type == CT_MESSAGE && ct->c_rfc934)
	goto skip_headers;

    /*
     * output the content type and subtype
     */
    np = add (TYPE_FIELD, NULL);
    vp = concat (" ", ci->ci_type, "/", ci->ci_subtype, NULL);

    /* keep track of length of line */
    len = strlen (TYPE_FIELD) + strlen (ci->ci_type)
		+ strlen (ci->ci_subtype) + 3;

    mailbody = ct->c_type == CT_MESSAGE
	&& ct->c_subtype == MESSAGE_EXTERNAL
	&& ((struct exbody *) ct->c_ctparams)->eb_body;

    /*
     * Append the attribute/value pairs to
     * the end of the Content-Type line.
     */
    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
	if (mailbody && !mh_strcasecmp (*ap, "body"))
	    continue;

	vp = add (";", vp);
	len++;

	snprintf (buffer, sizeof(buffer), "%s=\"%s\"", *ap, *ep);
	if (len + 1 + (cc = strlen (buffer)) >= CPERLIN) {
	    vp = add ("\n\t", vp);
	    len = 8;
	} else {
	    vp = add (" ", vp);
	    len++;
	}
	vp = add (buffer, vp);
	len += cc;
    }

    /*
     * Append any RFC-822 comment to the end of
     * the Content-Type line.
     */
    if (ci->ci_comment) {
	snprintf (buffer, sizeof(buffer), "(%s)", ci->ci_comment);
	if (len + 1 + (cc = 2 + strlen (ci->ci_comment)) >= CPERLIN) {
	    vp = add ("\n\t", vp);
	    len = 8;
	} else {
	    vp = add (" ", vp);
	    len++;
	}
	vp = add (buffer, vp);
	len += cc;
    }
    vp = add ("\n", vp);
    add_header (ct, np, vp);

    /*
     * output the Content-ID, unless disabled by -nocontentid
     */
    if (contentidsw && ct->c_id) {
	np = add (ID_FIELD, NULL);
	vp = concat (" ", ct->c_id, NULL);
	add_header (ct, np, vp);
    }

    /*
     * output the Content-Description
     */
    if (ct->c_descr) {
	np = add (DESCR_FIELD, NULL);
	vp = concat (" ", ct->c_descr, NULL);
	add_header (ct, np, vp);
    }

    /*
     * output the Content-Disposition
     */
    if (ct->c_dispo) {
	np = add (DISPO_FIELD, NULL);
	vp = concat (" ", ct->c_dispo, NULL);
	add_header (ct, np, vp);
    }

skip_headers:
    /*
     * If this is the internal content structure for a
     * "message/external", then we are done with the
     * headers (since it has no body).
     */
    if (ct->c_ctexbody)
	return OK;

    /*
     * output the Content-MD5
     */
    if (checksw) {
	np = add (MD5_FIELD, NULL);
	vp = calculate_digest (ct, (ct->c_encoding == CE_QUOTED) ? 1 : 0);
	add_header (ct, np, vp);
    }

    /*
     * output the Content-Transfer-Encoding
     */
    switch (ct->c_encoding) {
    case CE_7BIT:
	/* Nothing to output */
#if 0
	np = add (ENCODING_FIELD, NULL);
	vp = concat (" ", "7bit", "\n", NULL);
	add_header (ct, np, vp);
#endif
	break;

    case CE_8BIT:
	if (ct->c_type == CT_MESSAGE)
	    adios (NULL, "internal error, invalid encoding");

	np = add (ENCODING_FIELD, NULL);
	vp = concat (" ", "8bit", "\n", NULL);
	add_header (ct, np, vp);
	break;

    case CE_QUOTED:
	if (ct->c_type == CT_MESSAGE || ct->c_type == CT_MULTIPART)
	    adios (NULL, "internal error, invalid encoding");

	np = add (ENCODING_FIELD, NULL);
	vp = concat (" ", "quoted-printable", "\n", NULL);
	add_header (ct, np, vp);
	break;

    case CE_BASE64:
	if (ct->c_type == CT_MESSAGE || ct->c_type == CT_MULTIPART)
	    adios (NULL, "internal error, invalid encoding");

	np = add (ENCODING_FIELD, NULL);
	vp = concat (" ", "base64", "\n", NULL);
	add_header (ct, np, vp);
	break;

    case CE_BINARY:
	if (ct->c_type == CT_MESSAGE)
	    adios (NULL, "internal error, invalid encoding");

	np = add (ENCODING_FIELD, NULL);
	vp = concat (" ", "binary", "\n", NULL);
	add_header (ct, np, vp);
	break;

    default:
	adios (NULL, "unknown transfer encoding in content");
	break;
    }

    /*
     * Additional content specific header processing
     */
    switch (ct->c_type) {
    case CT_MULTIPART:
    {
	struct multipart *m;
	struct part *part;

	m = (struct multipart *) ct->c_ctparams;
	for (part = m->mp_parts; part; part = part->mp_next) {
	    CT p;

	    p = part->mp_part;
	    build_headers (p);
	}
    }
	break;

    case CT_MESSAGE:
	if (ct->c_subtype == MESSAGE_EXTERNAL) {
	    struct exbody *e;

	    e = (struct exbody *) ct->c_ctparams;
	    build_headers (e->eb_content);
	}
	break;

    default:
	/* Nothing to do */
	break;
    }

    return OK;
}


static char nib2b64[0x40+1] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *
calculate_digest (CT ct, int asciiP)
{
    int	cc;
    char buffer[BUFSIZ], *vp, *op;
    unsigned char *dp;
    unsigned char digest[16];
    unsigned char outbuf[25];
    MD5_CTX mdContext;
    CE ce = ct->c_cefile;
    char *infilename = ce->ce_file ? ce->ce_file : ct->c_file;
    FILE *in;

    /* open content */
    if ((in = fopen (infilename, "r")) == NULL)
	adios (infilename, "unable to open for reading");

    /* Initialize md5 context */
    MD5Init (&mdContext);

    /* calculate md5 message digest */
    if (asciiP) {
	while (fgets (buffer, sizeof(buffer) - 1, in)) {
	    char c, *cp;

	    cp = buffer + strlen (buffer) - 1;
	    if ((c = *cp) == '\n')
		*cp = '\0';

	    MD5Update (&mdContext, (unsigned char *) buffer,
		       (unsigned int) strlen (buffer));

	    if (c == '\n')
		MD5Update (&mdContext, (unsigned char *) "\r\n", 2);
	}
    } else {
	while ((cc = fread (buffer, sizeof(*buffer), sizeof(buffer), in)) > 0)
	    MD5Update (&mdContext, (unsigned char *) buffer, (unsigned int) cc);
    }

    /* md5 finalization.  Write digest and zero md5 context */
    MD5Final (digest, &mdContext);

    /* close content */
    fclose (in);

    /* print debugging info */
    if (debugsw) {
	unsigned char *ep;

	fprintf (stderr, "MD5 digest=");
	for (ep = (dp = digest) + sizeof(digest) / sizeof(digest[0]);
	         dp < ep; dp++)
	    fprintf (stderr, "%02x", *dp & 0xff);
	fprintf (stderr, "\n");
    }

    /* encode the digest using base64 */
    for (dp = digest, op = outbuf, cc = sizeof(digest) / sizeof(digest[0]);
		cc > 0; cc -= 3, op += 4) {
	unsigned long bits;
	char *bp;

	bits = (*dp++ & 0xff) << 16;
	if (cc > 1) {
	    bits |= (*dp++ & 0xff) << 8;
	    if (cc > 2)
		bits |= *dp++ & 0xff;
	}

	for (bp = op + 4; bp > op; bits >>= 6)
	    *--bp = nib2b64[bits & 0x3f];
	if (cc < 3) {
	    *(op + 3) = '=';
	    if (cc < 2)
		*(op + 2) = '=';
	}
    }

    /* null terminate string */
    outbuf[24] = '\0';

    /* now make copy and return string */
    vp = concat (" ", outbuf, "\n", NULL);
    return vp;
}
