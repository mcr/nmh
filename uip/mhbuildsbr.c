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
#include <h/md5.h>
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

extern int listsw;
extern int rfc934sw;
extern int contentidsw;

/* cache policies */
extern int rcachesw;	/* mhcachesbr.c */
extern int wcachesw;	/* mhcachesbr.c */

static char prefix[] = "----- =_aaaaaaaaaa";

struct attach_list {
    char *filename;
    struct attach_list *next;
};

typedef struct convert_list {
    char *type;
    char *filename;
    char *argstring;
    struct convert_list *next;
} convert_list;


/* mhmisc.c */
void content_error (char *, CT, char *, ...);

/* mhcachesbr.c */
int find_cache (CT, int, int *, char *, char *, int);

/* mhfree.c */
extern CT *cts;
void freects_done (int) NORETURN;
void free_ctinfo (CT);
void free_encoding (CT, int);

/*
 * static prototypes
 */
static int init_decoded_content (CT, const char *);
static void setup_attach_content(CT, char *);
static void set_disposition (CT);
static void set_charset (CT, int);
static void expand_pseudoheaders (CT, struct multipart *, const char *,
                                  const convert_list *);
static void expand_pseudoheader (CT, CT *, struct multipart *, const char *,
                                 const char *, const char *);
static char *fgetstr (char *, int, FILE *);
static int user_content (FILE *, char *, CT *, const char *infilename);
static void set_id (CT, int);
static int compose_content (CT, int);
static int scan_content (CT, size_t);
static int build_headers (CT, int);
static char *calculate_digest (CT, int);
static int extract_headers (CT, char *, FILE **);


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
build_mime (char *infile, int autobuild, int dist, int directives,
	    int header_encoding, size_t maxunencoded, int verbose)
{
    int	compnum, state;
    char buf[BUFSIZ], name[NAMESZ];
    char *cp, *np, *vp;
    struct multipart *m;
    struct part **pp;
    CT ct;
    FILE *in;
    HF hp;
    m_getfld_state_t gstate = 0;
    struct attach_list *attach_head = NULL, *attach_tail = NULL, *at_entry;
    convert_list *convert_head = NULL, *convert_tail = NULL, *convert;

    directive_init(directives);

    umask (~m_gmprot ());

    /* open the composition draft */
    if ((in = fopen (infile, "r")) == NULL)
	adios (infile, "unable to open for reading");

    /*
     * Allocate space for primary (outside) content
     */
    NEW0(ct);

    /*
     * Allocate structure for handling decoded content
     * for this part.  We don't really need this, but
     * allocate it to remain consistent.
     */
    init_decoded_content (ct, infile);

    /*
     * Parse some of the header fields in the composition
     * draft into the linked list of header fields for
     * the new MIME message.
     */
    m_getfld_track_filepos (&gstate, in);
    for (compnum = 1;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld (&gstate, name, buf, &bufsz, in)) {
	case FLD:
	case FLDPLUS:
	    compnum++;

	    /* abort if draft has Mime-Version or C-T-E header field */
	    if (strcasecmp (name, VRSN_FIELD) == 0 ||
		strcasecmp (name, ENCODING_FIELD) == 0) {
		if (autobuild) {
		    fclose(in);
		    free (ct);
		    return NULL;
		}
                adios (NULL, "draft shouldn't contain %s: field", name);
	    }

	    /* ignore any Content-Type fields in the header */
	    if (!strcasecmp (name, TYPE_FIELD)) {
		while (state == FLDPLUS) {
		    bufsz = sizeof buf;
		    state = m_getfld (&gstate, name, buf, &bufsz, in);
		}
		goto finish_field;
	    }

	    /* get copies of the buffers */
	    np = mh_xstrdup(name);
	    vp = mh_xstrdup(buf);

	    /* if necessary, get rest of field */
	    while (state == FLDPLUS) {
		bufsz = sizeof buf;
		state = m_getfld (&gstate, name, buf, &bufsz, in);
		vp = add (buf, vp);	/* add to previous value */
	    }

	    /*
	     * Now add the header data to the list, unless it's an attach
	     * header; in that case, add it to our attach list
	     */

	    if (strcasecmp(ATTACH_FIELD, np) == 0  ||
		strcasecmp(ATTACH_FIELD_ALT, np) == 0) {
		struct attach_list *entry;
		char *s = vp, *e = vp + strlen(vp) - 1;
		free(np);

		/*
		 * Make sure we can find the start of this filename.
		 * If it's blank, we skip completely.  Otherwise, strip
		 * off any leading spaces and trailing newlines.
		 */

		while (isspace((unsigned char) *s))
		    s++;

		while (e > s && *e == '\n')
		    *e-- = '\0';

		if (*s == '\0') {
		    free(vp);
		    goto finish_field;
		}

		NEW(entry);
		entry->filename = mh_xstrdup(s);
		entry->next = NULL;
		free(vp);

		if (attach_tail) {
		    attach_tail->next = entry;
		    attach_tail = entry;
		} else {
		    attach_head = attach_tail = entry;
		}
	    } else if (strncasecmp(MHBUILD_FILE_PSEUDOHEADER, np,
                                   strlen (MHBUILD_FILE_PSEUDOHEADER)) == 0) {
                /* E.g.,
                 * Nmh-mhbuild-file-text/calendar: /home/user/Mail/inbox/9
                 */
                char *type = np + strlen (MHBUILD_FILE_PSEUDOHEADER);
                char *filename = vp;

                /* vp should begin with a space because m_getfld()
                   includes the space after the colon in buf. */
                while (isspace((unsigned char) *filename)) { ++filename; }
                /* Trim trailing newline and any other whitespace. */
                rtrim (filename);

                for (convert = convert_head; convert; convert = convert->next) {
                    if (strcasecmp (convert->type, type) == 0) { break; }
                }
                if (convert) {
                    if (convert->filename  &&
                        strcasecmp (convert->filename, filename)) {
                        adios (NULL, "Multiple %s headers with different files"
                               " not allowed", type);
                    } else {
                        convert->filename = mh_xstrdup(filename);
                    }
                } else {
                    NEW0(convert);
                    convert->filename = mh_xstrdup(filename);
                    convert->type = mh_xstrdup(type);

                    if (convert_tail) {
                        convert_tail->next = convert;
                    } else {
                        convert_head = convert;
                    }
                    convert_tail = convert;
                }

                free (vp);
                free (np);
            } else if (strncasecmp(MHBUILD_ARGS_PSEUDOHEADER, np,
                                   strlen (MHBUILD_ARGS_PSEUDOHEADER)) == 0) {
                /* E.g.,
                 * Nmh-mhbuild-args-text/calendar: -reply accept
                 */
                char *type = np + strlen (MHBUILD_ARGS_PSEUDOHEADER);
                char *argstring = vp;

                /* vp should begin with a space because m_getfld()
                   includes the space after the colon in buf. */
                while (isspace((unsigned char) *argstring)) { ++argstring; }
                /* Trim trailing newline and any other whitespace. */
                rtrim (argstring);

                for (convert = convert_head; convert; convert = convert->next) {
                    if (strcasecmp (convert->type, type) == 0) { break; }
                }
                if (convert) {
                    if (convert->argstring  &&
                        strcasecmp (convert->argstring, argstring)) {
                        adios (NULL, "Multiple %s headers with different "
                               "argstrings not allowed", type);
                    } else {
                        convert->argstring = mh_xstrdup(argstring);
                    }
                } else {
                    NEW0(convert);
                    convert->type = mh_xstrdup(type);
                    convert->argstring = mh_xstrdup(argstring);

                    if (convert_tail) {
                        convert_tail->next = convert;
                    } else {
                        convert_head = convert;
                    }
                    convert_tail = convert;
                }

                free (vp);
                free (np);
	    } else {
		add_header (ct, np, vp);
	    }

finish_field:
	    /* if this wasn't the last header field, then continue */
	    continue;

	case BODY:
	    fseek (in, (long) (-strlen (buf)), SEEK_CUR);
	    /* FALLTHRU */
	case FILEEOF:
	    break;

	case LENERR:
	case FMTERR:
	    adios (NULL, "message format error in component #%d", compnum);

	default:
	    adios (NULL, "getfld() returned %d", state);
	}
	break;
    }
    m_getfld_state_destroy (&gstate);

    if (header_encoding != CE_8BIT) {
        /*
         * Iterate through the list of headers and call the function to MIME-ify
         * them if required.
         */

        for (hp = ct->c_first_hf; hp != NULL; hp = hp->next) {
            if (encode_rfc2047(hp->name, &hp->value, header_encoding, NULL)) {
                adios(NULL, "Unable to encode header \"%s\"", hp->name);
            }
        }
    }

    /*
     * Now add the MIME-Version header field
     * to the list of header fields.
     */

    if (! dist) {
	np = mh_xstrdup(VRSN_FIELD);
	vp = concat (" ", VRSN_VALUE, "\n", NULL);
	add_header (ct, np, vp);
    }

    /*
     * We initally assume we will find multiple contents in the
     * draft.  So create a multipart/mixed content to hold everything.
     * We can remove this later, if it is not needed.
     */
    if (get_ctinfo ("multipart/mixed", ct, 0) == NOTOK)
	done (1);
    ct->c_type = CT_MULTIPART;
    ct->c_subtype = MULTI_MIXED;

    NEW0(m);
    ct->c_ctparams = (void *) m;
    pp = &m->mp_parts;

    /*
     * read and parse the composition file
     * and the directives it contains.
     */
    while (fgetstr (buf, sizeof(buf) - 1, in)) {
	struct part *part;
	CT p;

	if (user_content (in, buf, &p, infile) == DONE) {
	    inform("ignoring spurious #end, continuing...");
	    continue;
	}
	if (!p)
	    continue;

	NEW0(part);
	*pp = part;
	pp = &part->mp_next;
	part->mp_part = p;
    }

    /*
     * Add any Attach headers to the list of MIME parts at the end of the
     * message.
     */

    for (at_entry = attach_head; at_entry; ) {
	struct attach_list *at_prev = at_entry;
	struct part *part;
	CT p;

	if (access(at_entry->filename, R_OK) != 0) {
	    adios("reading", "Unable to open %s for", at_entry->filename);
	}

	NEW0(p);
	init_decoded_content(p, infile);

	/*
	 * Initialize our content structure based on the filename,
	 * and fill in all of the relevant fields.  Also place MIME
	 * parameters in the attributes array.
	 */

	setup_attach_content(p, at_entry->filename);

	NEW0(part);
	*pp = part;
	pp = &part->mp_next;
	part->mp_part = p;

	at_entry = at_entry->next;
	free(at_prev->filename);
	free(at_prev);
    }

    /*
     * Handle the mhbuild pseudoheaders, which deal with specific
     * content types.
     */
    if (convert_head) {
        CT *ctp;
        convert_list *next;

        done = freects_done;

        /* In case there are multiple calls that land here, prevent leak. */
        for (ctp = cts; ctp && *ctp; ++ctp) { free_content (*ctp); }
        free (cts);

        /* Extract the type part (as a CT) from filename. */
        cts = mh_xcalloc(2, sizeof *cts);
        if (! (cts[0] = parse_mime (convert_head->filename))) {
            adios (NULL, "failed to parse %s", convert_head->filename);
        }

        expand_pseudoheaders (cts[0], m, infile, convert_head);

        /* Free the convert list. */
        for (convert = convert_head; convert; convert = next) {
            next = convert->next;
            free (convert->type);
            free (convert->filename);
            free (convert->argstring);
            free (convert);
        }
        convert_head = NULL;
    }

    /*
     * To allow for empty message bodies, if we've found NO content at all
     * yet cook up an empty text/plain part.
     */

    if (!m->mp_parts) {
	CT p;
	struct part *part;
	struct text *t;

	NEW0(p);
	init_decoded_content(p, infile);

	if (get_ctinfo ("text/plain", p, 0) == NOTOK)
	    done (1);

	p->c_type = CT_TEXT;
	p->c_subtype = TEXT_PLAIN;
	p->c_encoding = CE_7BIT;
	/*
	 * Sigh.  ce_file contains the "decoded" contents of this part.
	 * So this seems like the best option available since we're going
	 * to call scan_content() on this.
	 */
	p->c_cefile.ce_file = mh_xstrdup("/dev/null");
	p->c_begin = ftell(in);
	p->c_end = ftell(in);

	NEW0(t);
	t->tx_charset = CHARSET_SPECIFIED;
	p->c_ctparams = t;

	NEW0(part);
	*pp = part;
	part->mp_part = p;
    }

    /*
     * close the composition draft since
     * it's not needed any longer.
     */
    fclose (in);

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
    compose_content (ct, verbose);

    if ((cp = strchr(prefix, 'a')) == NULL)
	adios (NULL, "internal error(4)");

    /*
     * If using EAI, force 8-bit charset.
     */
    if (header_encoding == CE_8BIT) {
        set_charset (ct, 1);
    }

    /*
     * Scan the contents.  Choose a transfer encoding, and
     * check if prefix for multipart boundary clashes with
     * any of the contents.
     */
    while (scan_content (ct, maxunencoded) == NOTOK) {
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
    if (! dist)
	build_headers (ct, header_encoding);

    return ct;
}


/*
 * Set up structures for placing unencoded
 * content when building parts.
 */

static int
init_decoded_content (CT ct, const char *filename)
{
    ct->c_ceopenfnx  = open7Bit;	/* since unencoded */
    ct->c_ceclosefnx = close_encoding;
    ct->c_cesizefnx  = NULL;		/* since unencoded */
    ct->c_encoding = CE_7BIT;		/* Seems like a reasonable default */
    ct->c_file = add(filename, NULL);

    return OK;
}


static char *
fgetstr (char *s, int n, FILE *stream)
{
    char *cp, *ep;

    ep = s + n;
    while(1) {
	for (cp = s; cp < ep;) {
	    int len;

	    if (!fgets (cp, n, stream))
                return cp == s ? NULL : s; /* "\\\nEOF" ignored. */

	    if (! do_direct()  ||  (cp == s && *cp != '#'))
		return s; /* Plaintext line. */

	    len = strlen(cp);
	    if (len <= 1)
		break; /* Can't contain "\\\n". */
	    cp += len - 1; /* Just before NUL. */
	    if (*cp-- != '\n' || *cp != '\\')
		break;
	    *cp = '\0'; /* Erase the trailing "\\\n". */
	    n -= (len - 2);
	}

	if (strcmp(s, "#on\n") == 0) {
	    directive_onoff(1);
	} else if (strcmp(s, "#off\n") == 0) {
	    directive_onoff(0);
	} else if (strcmp(s, "#pop\n") == 0) {
	    directive_pop();
	} else {
	    return s;
	}
    }
}


/*
 * Parse the composition draft for text and directives.
 * Do initial setup of Content structure.
 */

static int
user_content (FILE *in, char *buf, CT *ctp, const char *infilename)
{
    int	extrnal, vrsn;
    char *cp, **ap;
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
    NEW0(ct);
    *ctp = ct;

    /* allocate basic structure for handling decoded content */
    init_decoded_content (ct, infilename);
    ce = &ct->c_cefile;

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

	if ((cp = m_mktemp2(NULL, invo_name, NULL, &out)) == NULL) {
	    adios("mhbuildsbr", "unable to create temporary file in %s",
		  get_temp_dir());
	}

	/* use a temp file to collect the plain text lines */
	ce->ce_file = mh_xstrdup(cp);
	ce->ce_unlink = 1;

	if (do_direct() && (buf[0] == '#' && buf[1] == '<')) {
	    strncpy (content, buf + 2, sizeof(content));
	    inlineD = 1;
	    goto rock_and_roll;
	}
        inlineD = 0;

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
	    if (!strcasecmp (ci->ci_type, s2i->si_key))
		break;
	if (!s2i->si_key && !uprf (ci->ci_type, "X-"))
	    s2i++;

	/*
	 * check type specified (possibly implicitly)
	 */
	switch (ct->c_type = s2i->si_val) {
	case CT_MESSAGE:
	    if (!strcasecmp (ci->ci_subtype, "rfc822")) {
		ct->c_encoding = CE_7BIT;
		goto call_init;
	    }
	    /* FALLTHRU */
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
	if (!strcasecmp (ci->ci_type, s2i->si_key))
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
	    if (!strcasecmp (ci->ci_subtype, "partial"))
		adios (NULL, "sorry, \"#%s/%s\" isn't supported",
		       ci->ci_type, ci->ci_subtype);
	    if (!strcasecmp (ci->ci_subtype, "external-body"))
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
	    NEW0(ct);
	    init_decoded_content(ct, infilename);
	    *ctp = ct;
	    if (get_ctinfo (buffer, ct, 0) == NOTOK)
		done (1);
	    ct->c_type = CT_MESSAGE;
	    ct->c_subtype = MESSAGE_EXTERNAL;

	    NEW0(e);
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
		for (cp = ci->ci_magic + 1; isspace ((unsigned char) *cp); cp++)
		    continue;
		if (!*cp)
		    adios (NULL, "empty pipe command for #%s directive", ci->ci_type);
		cp = mh_xstrdup(cp);
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
	cp = context_find_by_type ("compose", ci->ci_type, ci->ci_subtype);
	if (cp == NULL) {
	    content_error (NULL, ct, "don't know how to compose content");
	    done (1);
	}
	ci->ci_magic = mh_xstrdup(cp);
	return OK;
    }

    if (extrnal)
	adios (NULL, "external definition not allowed for \"#%s\"", ci->ci_type);

    /*
     * Message directive
     * #forw [+folder] [msgs]
     */
    if (!strcasecmp (ci->ci_type, "forw")) {
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
	    folder = mh_xstrdup(getfolder(1));

	if (!(mp = folder_read (folder, 0)))
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

	    NEW0(m);
	    ct->c_ctparams = (void *) m;
	    pp = &m->mp_parts;

	    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
		if (is_selected(mp, msgnum)) {
		    struct part *part;
		    CT p;
		    CE pe;

		    NEW0(p);
		    init_decoded_content (p, infilename);
		    pe = &p->c_cefile;
		    if (get_ctinfo ("message/rfc822", p, 0) == NOTOK)
			done (1);
		    p->c_type = CT_MESSAGE;
		    p->c_subtype = MESSAGE_RFC822;

		    snprintf (buffer, sizeof(buffer), "%s/%d", mp->foldpath, msgnum);
		    pe->ce_file = mh_xstrdup(buffer);
		    if (listsw && stat (pe->ce_file, &st) != NOTOK)
			p->c_end = (long) st.st_size;

		    NEW0(part);
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
	    ce->ce_file = mh_xstrdup(buffer);
	    if (listsw && stat (ce->ce_file, &st) != NOTOK)
		ct->c_end = (long) st.st_size;
	}

	folder_free (mp);	/* free folder/message structure */
	return OK;
    }

    /*
     * #end
     */
    if (!strcasecmp (ci->ci_type, "end")) {
	free_content (ct);
	*ctp = NULL;
	return DONE;
    }

    /*
     * #begin [ alternative | parallel ]
     */
    if (!strcasecmp (ci->ci_type, "begin")) {
	if (!ci->ci_magic) {
	    vrsn = MULTI_MIXED;
	    cp = SubMultiPart[vrsn - 1].kv_key;
	} else if (!strcasecmp (ci->ci_magic, "alternative")) {
	    vrsn = MULTI_ALTERNATE;
	    cp = SubMultiPart[vrsn - 1].kv_key;
	} else if (!strcasecmp (ci->ci_magic, "parallel")) {
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

	NEW0(m);
	ct->c_ctparams = (void *) m;

	pp = &m->mp_parts;
	while (fgetstr (buffer, sizeof(buffer) - 1, in)) {
	    struct part *part;
	    CT p;

	    if (user_content (in, buffer, &p, infilename) == DONE) {
		if (!m->mp_parts)
		    adios (NULL, "empty \"#begin ... #end\" sequence");
		return OK;
	    }
	    if (!p)
		continue;

	    NEW0(part);
	    *pp = part;
	    pp = &part->mp_next;
	    part->mp_part = p;
	}
	inform("premature end-of-file, missing #end, continuing...");
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
	msgfmt = mh_xstrdup(contentid);
    }
    snprintf (contentid, sizeof(contentid), msgfmt, top ? 0 : ++partno);
    ct->c_id = mh_xstrdup(contentid);
}


/*
 * Fill out, or expand the various contents in the composition
 * draft.  Read-in any necessary files.  Parse and execute any
 * commands specified by profile composition strings.
 */

static int
compose_content (CT ct, int verbose)
{
    CE ce = &ct->c_cefile;

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
	    p->c_partno = mh_xstrdup(partnam);
	    if (compose_content (p, verbose) == NOTOK)
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
	    char *bp, *cp;
	    char *vec[4], buffer[BUFSIZ];
	    FILE *out;
	    CI ci = &ct->c_ctinfo;
            char *tfile = NULL;

	    if (!(cp = ci->ci_magic))
		adios (NULL, "internal error(5)");

	    if ((tfile = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
		adios("mhbuildsbr", "unable to create temporary file in %s",
		      get_temp_dir());
	    }
	    ce->ce_file = mh_xstrdup(tfile);
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
			char *s = "";
			PM pm;

			for (pm = ci->ci_first_pm; pm; pm = pm->pm_next) {
			    snprintf (bp, buflen, "%s%s=\"%s\"", s,
				    pm->pm_name, get_param_value(pm, '?'));
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
			/* FALLTHRU */

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

	    if (verbose)
		printf ("composing content %s/%s from command\n\t%s\n",
			ci->ci_type, ci->ci_subtype, buffer);

	    fflush (stdout);	/* not sure if need for -noverbose */

	    vec[0] = "/bin/sh";
	    vec[1] = "-c";
	    vec[2] = buffer;
	    vec[3] = NULL;

	    if ((out = fopen (ce->ce_file, "w")) == NULL)
		adios (ce->ce_file, "unable to open for writing");

	    for (i = 0; (child_id = fork()) == NOTOK && i > 5; i++)
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
scan_content (CT ct, size_t maxunencoded)
{
    int prefix_len;
    int check8bit = 0, contains8bit = 0;  /* check if contains 8bit data */
    int checknul = 0, containsnul = 0;  /* check if contains NULs */
    int checklinelen = 0, linelen = 0;  /* check for long lines */
    int checkllinelen = 0; /* check for extra-long lines */
    int checkboundary = 0, boundaryclash = 0; /* check if clashes with multipart boundary   */
    int checklinespace = 0, linespace = 0;  /* check if any line ends with space          */
    char *cp = NULL;
    char *bufp = NULL;
    size_t buflen;
    ssize_t gotlen;
    struct text *t = NULL;
    FILE *in = NULL;
    CE ce = &ct->c_cefile;

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

	    if (scan_content (p, maxunencoded) == NOTOK)	/* choose encoding for subpart */
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
     * Decide what to check while scanning this content.  Note that
     * for text content we always check for 8bit characters if the
     * charset is unspecified, because that controls whether or not the
     * character set is us-ascii or retrieved from the locale.  And
     * we check even if the charset is specified, to allow setting
     * the proper Content-Transfer-Encoding.
     */

    if (ct->c_type == CT_TEXT) {
	t = (struct text *) ct->c_ctparams;
	if (t->tx_charset == CHARSET_UNSPECIFIED) {
	    checknul = 1;
	}
	check8bit = 1;
    }

    switch (ct->c_reqencoding) {
    case CE_8BIT:
	checkllinelen = 1;
	checkboundary = 1;
	break;
    case CE_QUOTED:
	checkboundary = 1;
	break;
    case CE_BASE64:
	break;
    case CE_UNKNOWN:
	/* Use the default rules based on content-type */
	switch (ct->c_type) {
	case CT_TEXT:
	    checkboundary = 1;
	    checklinelen = 1;
	    if (ct->c_subtype == TEXT_PLAIN) {
		checklinespace = 0;
	    } else {
		checklinespace = 1;
	    }
	break;

	case CT_APPLICATION:
	    check8bit = 1;
	    checknul = 1;
	    checklinelen = 1;
	    checklinespace = 1;
	    checkboundary = 1;
	break;

	case CT_MESSAGE:
	    checklinelen = 0;
	    checklinespace = 0;

	    /* don't check anything for message/external */
	    if (ct->c_subtype == MESSAGE_EXTERNAL) {
		checkboundary = 0;
		check8bit = 0;
	    } else {
		checkboundary = 1;
		check8bit = 1;
	    }
	    break;

	case CT_AUDIO:
	case CT_IMAGE:
	case CT_VIDEO:
	    /*
	     * Don't check anything for these types,
	     * since we are forcing use of base64, unless
	     * the content-type was specified by a mhbuild directive.
	     */
	    check8bit = 0;
	    checklinelen = 0;
	    checklinespace = 0;
	    checkboundary = 0;
	    break;
	}
    }

    /*
     * Scan the unencoded content
     */
    if (check8bit || checklinelen || checklinespace || checkboundary ||
	checkllinelen || checknul) {
	if ((in = fopen (ce->ce_file, "r")) == NULL)
	    adios (ce->ce_file, "unable to open for reading");
	prefix_len = strlen (prefix);

	while ((gotlen = getline(&bufp, &buflen, in)) != -1) {
	    /*
	     * Check for 8bit and NUL data.
	     */
	    for (cp = bufp; (check8bit || checknul) &&
					cp < bufp + gotlen; cp++) {
		if (!isascii ((unsigned char) *cp)) {
		    contains8bit = 1;
		    check8bit = 0;	/* no need to keep checking */
		}
		if (!*cp) {
		    containsnul = 1;
		    checknul = 0;	/* no need to keep checking */
		}
	    }

	    /*
	     * Check line length.
	     */
	    if (checklinelen && ((size_t)gotlen > maxunencoded + 1)) {
		linelen = 1;
		checklinelen = 0;	/* no need to keep checking */
	    }

	    /*
	     * RFC 5322 specifies that a message cannot contain a line
	     * greater than 998 characters (excluding the CRLF).  If we
	     * get one of those lines and linelen is NOT set, then abort.
	     */

	    if (checkllinelen && !linelen &&
					(gotlen > MAXLONGLINE + 1)) {
		adios(NULL, "Line in content exceeds maximum line limit (%d)",
		      MAXLONGLINE);
	    }

	    /*
	     * Check if line ends with a space.
	     */
	    if (checklinespace && (cp = bufp + gotlen - 2) > bufp &&
			isspace ((unsigned char) *cp)) {
		linespace = 1;
		checklinespace = 0;	/* no need to keep checking */
	    }

	    /*
	     * Check if content contains a line that clashes
	     * with our standard boundary for multipart messages.
	     */
	    if (checkboundary && bufp[0] == '-' && bufp[1] == '-') {
		for (cp = bufp + gotlen - 1; cp >= bufp; cp--)
		    if (!isspace ((unsigned char) *cp))
			break;
		*++cp = '\0';
		if (!strncmp(bufp + 2, prefix, prefix_len) &&
			    isdigit((unsigned char) bufp[2 + prefix_len])) {
		    boundaryclash = 1;
		    checkboundary = 0;	/* no need to keep checking */
		}
	    }
	}
	fclose (in);
	free(bufp);
    }

    /*
     * If the content is text and didn't specify a character set,
     * we need to figure out which one was used.
     */
    set_charset (ct, contains8bit);

    /*
     * Decide which transfer encoding to use.
     */

    if (ct->c_reqencoding != CE_UNKNOWN)
	ct->c_encoding = ct->c_reqencoding;
    else {
	int wants_q_p = (containsnul || linelen || linespace || checksw);

	switch (ct->c_type) {
	case CT_TEXT:
            if (wants_q_p)
                 ct->c_encoding = CE_QUOTED;
            else if (contains8bit)
                 ct->c_encoding = CE_8BIT;
            else
                 ct->c_encoding = CE_7BIT;

	    break;

	case CT_APPLICATION:
	    /* For application type, use base64, except when postscript */
	    if (wants_q_p || contains8bit) {
		if (ct->c_subtype == APPLICATION_POSTSCRIPT)
		    ct->c_encoding = CE_QUOTED;  /* historical */
		else
		    ct->c_encoding = CE_BASE64;
	    } else {
		ct->c_encoding = CE_7BIT;
	    }
	    break;

	case CT_MESSAGE:
	    ct->c_encoding = contains8bit ? CE_8BIT : CE_7BIT;
	    break;

	case CT_AUDIO:
	case CT_IMAGE:
	case CT_VIDEO:
	    /* For audio, image, and video contents, just use base64 */
	    ct->c_encoding = CE_BASE64;
	    break;
        }
    }

    return (boundaryclash ? NOTOK : OK);
}


/*
 * Scan the content structures, and build header
 * fields that will need to be output into the
 * message.
 */

static int
build_headers (CT ct, int header_encoding)
{
    int	cc, mailbody, extbody, len;
    char *np, *vp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;

    /*
     * If message is type multipart, then add the multipart
     * boundary to the list of attribute/value pairs.
     */
    if (ct->c_type == CT_MULTIPART) {
	static int level = 0;	/* store nesting level */

	snprintf (buffer, sizeof(buffer), "%s%d", prefix, level++);
	add_param(&ci->ci_first_pm, &ci->ci_last_pm, "boundary", buffer, 0);
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
    np = mh_xstrdup(TYPE_FIELD);
    vp = concat (" ", ci->ci_type, "/", ci->ci_subtype, NULL);

    /* keep track of length of line */
    len = strlen (TYPE_FIELD) + strlen (ci->ci_type)
		+ strlen (ci->ci_subtype) + 3;

    extbody = ct->c_type == CT_MESSAGE && ct->c_subtype == MESSAGE_EXTERNAL;
    mailbody = extbody && ((struct exbody *) ct->c_ctparams)->eb_body;

    /*
     * Append the attribute/value pairs to
     * the end of the Content-Type line.
     */

    if (ci->ci_first_pm) {
	char *s = output_params(len, ci->ci_first_pm, &len, mailbody);

	if (!s)
	    adios(NULL, "Internal error: failed outputting Content-Type "
		"parameters");

	vp = add (s, vp);
	free(s);
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
     * output the Content-ID, unless disabled by -nocontentid.  Note that
     * RFC 2045 always requires a Content-ID header for message/external-body
     * entities.
     */
    if ((contentidsw || ct->c_ctexbody) && ct->c_id) {
	np = mh_xstrdup(ID_FIELD);
	vp = concat (" ", ct->c_id, NULL);
	add_header (ct, np, vp);
    }
    /*
     * output the Content-Description
     */
    if (ct->c_descr) {
	np = mh_xstrdup(DESCR_FIELD);
	vp = concat (" ", ct->c_descr, NULL);
	if (header_encoding != CE_8BIT) {
	    if (encode_rfc2047(DESCR_FIELD, &vp, header_encoding, NULL)) {
		adios(NULL, "Unable to encode %s header", DESCR_FIELD);
	    }
	}
	add_header (ct, np, vp);
    }

    /*
     * output the Content-Disposition.  If it's NULL but c_dispo_type is
     * set, then we need to build it.
     */
    if (ct->c_dispo) {
	np = mh_xstrdup(DISPO_FIELD);
	vp = concat (" ", ct->c_dispo, NULL);
	add_header (ct, np, vp);
    } else if (ct->c_dispo_type) {
	vp = concat (" ", ct->c_dispo_type, NULL);
	len = strlen(DISPO_FIELD) + strlen(vp) + 1;
	np = output_params(len, ct->c_dispo_first, NULL, 0);
	vp = add(np, vp);
	vp = add("\n", vp);
        mh_xfree(np);
	add_header (ct, mh_xstrdup(DISPO_FIELD), vp);
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
	np = mh_xstrdup(MD5_FIELD);
	vp = calculate_digest (ct, (ct->c_encoding == CE_QUOTED) ? 1 : 0);
	add_header (ct, np, vp);
    }

    /*
     * output the Content-Transfer-Encoding
     * If using EAI and message body is 7-bit, force 8-bit C-T-E.
     */
    if (header_encoding == CE_8BIT  &&  ct->c_encoding == CE_7BIT) {
        ct->c_encoding = CE_8BIT;
    }

    switch (ct->c_encoding) {
    case CE_7BIT:
	/* Nothing to output */
	break;

    case CE_8BIT:
	np = mh_xstrdup(ENCODING_FIELD);
	vp = concat (" ", "8bit", "\n", NULL);
	add_header (ct, np, vp);
	break;

    case CE_QUOTED:
	if (ct->c_type == CT_MESSAGE || ct->c_type == CT_MULTIPART)
	    adios (NULL, "internal error, invalid encoding");

	np = mh_xstrdup(ENCODING_FIELD);
	vp = concat (" ", "quoted-printable", "\n", NULL);
	add_header (ct, np, vp);
	break;

    case CE_BASE64:
	if (ct->c_type == CT_MESSAGE || ct->c_type == CT_MULTIPART)
	    adios (NULL, "internal error, invalid encoding");

	np = mh_xstrdup(ENCODING_FIELD);
	vp = concat (" ", "base64", "\n", NULL);
	add_header (ct, np, vp);
	break;

    case CE_BINARY:
	if (ct->c_type == CT_MESSAGE)
	    adios (NULL, "internal error, invalid encoding");

	np = mh_xstrdup(ENCODING_FIELD);
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
	    build_headers (p, header_encoding);
	}
    }
	break;

    case CT_MESSAGE:
	if (ct->c_subtype == MESSAGE_EXTERNAL) {
	    struct exbody *e;

	    e = (struct exbody *) ct->c_ctparams;
	    build_headers (e->eb_content, header_encoding);
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
    char *vp, *op;
    unsigned char *dp;
    unsigned char digest[16];
    unsigned char outbuf[25];
    MD5_CTX mdContext;
    CE ce = &ct->c_cefile;
    char *infilename = ce->ce_file ? ce->ce_file : ct->c_file;
    FILE *in;

    /* open content */
    if ((in = fopen (infilename, "r")) == NULL)
	adios (infilename, "unable to open for reading");

    /* Initialize md5 context */
    MD5Init (&mdContext);

    /* calculate md5 message digest */
    if (asciiP) {
	char *bufp = NULL;
	size_t buflen;
	ssize_t gotlen;
	while ((gotlen = getline(&bufp, &buflen, in)) != -1) {
	    char c, *cp;

	    cp = bufp + gotlen - 1;
	    if ((c = *cp) == '\n')
		gotlen--;

	    MD5Update (&mdContext, (unsigned char *) bufp,
		       (unsigned int) gotlen);

	    if (c == '\n')
		MD5Update (&mdContext, (unsigned char *) "\r\n", 2);
	}
    } else {
	char buffer[BUFSIZ];
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
    for (dp = digest, op = (char *) outbuf,
				cc = sizeof(digest) / sizeof(digest[0]);
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

/*
 * Set things up for the content structure for file "filename" that
 * we want to attach
 */

static void
setup_attach_content(CT ct, char *filename)
{
    char *type, *simplename = r1bindex(filename, '/');
    struct str2init *s2i;
    PM pm;

    if (! (type = mime_type(filename))) {
	adios(NULL, "Unable to determine MIME type of \"%s\"", filename);
    }

    /*
     * Parse the Content-Type.  get_ctinfo() parses MIME parameters, but
     * since we're just feeding it a MIME type we have to add those ourself.
     * Map that to a valid content-type label and call any initialization
     * function.
     */

    if (get_ctinfo(type, ct, 0) == NOTOK)
	done(1);

    free(type);

    for (s2i = str2cts; s2i->si_key; s2i++)
	if (strcasecmp(ct->c_ctinfo.ci_type, s2i->si_key) == 0)
	    break;
    if (!s2i->si_key && !uprf(ct->c_ctinfo.ci_type, "X-"))
	s2i++;

    /*
     * Make sure the type isn't incompatible with what we can handle
     */

    switch (ct->c_type = s2i->si_val) {
    case CT_MULTIPART:
	adios (NULL, "multipart types must be specified by mhbuild directives");
	/* NOTREACHED */

    case CT_MESSAGE:
	if (strcasecmp(ct->c_ctinfo.ci_subtype, "partial") == 0)
	    adios(NULL, "Sorry, %s/%s isn't supported", ct->c_ctinfo.ci_type,
		ct->c_ctinfo.ci_subtype);
	if (strcasecmp(ct->c_ctinfo.ci_subtype, "external-body") == 0)
	    adios(NULL, "external-body messages must be specified "
		"by mhbuild directives");
	/* FALLTHRU */

    default:
	/*
	 * This sets the subtype, if it's significant
	 */
	if ((ct->c_ctinitfnx = s2i->si_init))
	    (*ct->c_ctinitfnx)(ct);
	break;
    }

    /*
     * Feed in a few attributes; specifically, the name attribute, the
     * content-description, and the content-disposition.
     */

    for (pm = ct->c_ctinfo.ci_first_pm; pm; pm = pm->pm_next) {
	if (strcasecmp(pm->pm_name, "name") == 0) {
            mh_xfree(pm->pm_value);
	    pm->pm_value = mh_xstrdup(simplename);
	    break;
	}
    }

    if (pm == NULL)
	add_param(&ct->c_ctinfo.ci_first_pm, &ct->c_ctinfo.ci_last_pm,
		  "name", simplename, 0);

    ct->c_descr = mh_xstrdup(simplename);
    ct->c_descr = add("\n", ct->c_descr);
    ct->c_cefile.ce_file = mh_xstrdup(filename);

    set_disposition (ct);

    add_param(&ct->c_dispo_first, &ct->c_dispo_last, "filename", simplename, 0);
}

/*
 * If disposition type hasn't already been set in ct:
 * Look for mhbuild-disposition-<type>/<subtype> entry
 * that specifies Content-Disposition type.  Only
 * 'attachment' and 'inline' are allowed.  Default to
 * 'attachment'.
 */
void
set_disposition (CT ct) {
    if (ct->c_dispo_type == NULL) {
        char *cp = context_find_by_type ("disposition", ct->c_ctinfo.ci_type,
                                         ct->c_ctinfo.ci_subtype);

        if (cp  &&  strcasecmp (cp, "attachment")  &&
            strcasecmp (cp, "inline")) {
            inform("configuration problem: %s-disposition-%s%s%s specifies "
		"'%s' but only 'attachment' and 'inline' are allowed, "
		"continuing...", invo_name,
		ct->c_ctinfo.ci_type,
		ct->c_ctinfo.ci_subtype ? "/" : "",
		ct->c_ctinfo.ci_subtype ? ct->c_ctinfo.ci_subtype : "",
		cp);
        }

        if (!cp)
            cp = "attachment";
        ct->c_dispo_type = mh_xstrdup(cp);
    }
}

/*
 * Set text content charset if it was unspecified.  contains8bit
 * selctions:
 * 0: content does not contain 8-bit characters
 * 1: content contains 8-bit characters
 * -1: ignore content and use user's locale to determine charset
 */
void
set_charset (CT ct, int contains8bit) {
    if (ct->c_type == CT_TEXT) {
        struct text *t;

        if (ct->c_ctparams == NULL) {
            NEW0(t);
            ct->c_ctparams = t;
            t->tx_charset = CHARSET_UNSPECIFIED;
        } else {
            t = (struct text *) ct->c_ctparams;
        }

        if (t->tx_charset == CHARSET_UNSPECIFIED) {
            CI ci = &ct->c_ctinfo;
            char *eightbitcharset = write_charset_8bit();
            char *charset = contains8bit ? eightbitcharset : "us-ascii";

            if (contains8bit == 1  &&
                strcasecmp (eightbitcharset, "US-ASCII") == 0) {
                adios (NULL, "Text content contains 8 bit characters, but "
                       "character set is US-ASCII");
            }

            add_param (&ci->ci_first_pm, &ci->ci_last_pm, "charset", charset,
                       0);

            t->tx_charset = CHARSET_SPECIFIED;
        }
    }
}


/*
 * Look at all of the replied-to message parts and expand any that
 * are matched by a pseudoheader.  Except don't descend into
 * message parts.
 */
void
expand_pseudoheaders (CT ct, struct multipart *m, const char *infile,
                      const convert_list *convert_head) {
    /* text_plain_ct is used to concatenate all of the text/plain
       replies into one part, instead of having each one in a separate
       part. */
    CT text_plain_ct = NULL;

    switch (ct->c_type) {
    case CT_MULTIPART: {
        struct multipart *mp = (struct multipart *) ct->c_ctparams;
        struct part *part;

        if (ct->c_subtype == MULTI_ALTERNATE) {
            int matched = 0;

            /* The parts are in descending priority order (defined by
               RFC 2046 Sec. 5.1.4) because they were reversed by
               parse_mime ().  So, stop looking for matches with
               immediate subparts after the first match of an
               alternative. */
            for (part = mp->mp_parts; ! matched && part; part = part->mp_next) {
                char *type_subtype =
                    concat (part->mp_part->c_ctinfo.ci_type, "/",
                            part->mp_part->c_ctinfo.ci_subtype, NULL);

                if (part->mp_part->c_type == CT_MULTIPART) {
                    expand_pseudoheaders (part->mp_part, m, infile,
                                          convert_head);
                } else {
                    const convert_list *c;

                    for (c = convert_head; c; c = c->next) {
                        if (! strcasecmp (type_subtype, c->type)) {
                            expand_pseudoheader (part->mp_part, &text_plain_ct,
                                                 m, infile,
                                                 c->type, c->argstring);
                            matched = 1;
                            break;
                        }
                    }
                }
                free (type_subtype);
            }
        } else {
            for (part = mp->mp_parts; part; part = part->mp_next) {
                expand_pseudoheaders (part->mp_part, m, infile, convert_head);
            }
        }
        break;
    }

    default: {
        char *type_subtype =
            concat (ct->c_ctinfo.ci_type, "/", ct->c_ctinfo.ci_subtype,
                    NULL);
        const convert_list *c;

        for (c = convert_head; c; c = c->next) {
            if (! strcasecmp (type_subtype, c->type)) {
                expand_pseudoheader (ct, &text_plain_ct, m, infile, c->type,
                                     c->argstring);
                break;
            }
        }
        free (type_subtype);
        break;
    }
    }
}


/*
 * Expand a single pseudoheader.  It's for the specified type.
 */
void
expand_pseudoheader (CT ct, CT *text_plain_ct, struct multipart *m,
                     const char *infile, const char *type,
                     const char *argstring) {
    char *reply_file;
    FILE *reply_fp = NULL;
    char *convert, *type_p, *subtype_p;
    char *convert_command;
    char *charset = NULL;
    char *cp;
    struct str2init *s2i;
    CT reply_ct;
    struct part *part;
    int eightbit = 0;
    int status;

    type_p = getcpy (type);
    if ((subtype_p = strchr (type_p, '/'))) {
        *subtype_p++ = '\0';
        convert = context_find_by_type ("convert", type_p, subtype_p);
    } else {
        free (type_p);
        type_p = concat ("mhbuild-convert-", type, NULL);
        convert = context_find (type_p);
    }
    free (type_p);

    if (! (convert)) {
        /* No mhbuild-convert- entry in mhn.defaults or profile
           for type. */
        return;
    }
    /* reply_file is used to pass the output of the convert. */
    reply_file = getcpy (m_mktemp2 (NULL, invo_name, NULL, NULL));
    convert_command =
        concat (convert, " ", argstring ? argstring : "", " >", reply_file,
                NULL);

    /* Convert here . . . */
    ct->c_storeproc = mh_xstrdup(convert_command);
    ct->c_umask = ~m_gmprot ();

    if ((status = show_content_aux (ct, 0, convert_command, NULL, NULL)) !=
        OK) {
        inform("store of %s content failed, continuing...", type);
    }
    free (convert_command);

    /* Fill out the the new ct, reply_ct. */
    NEW0(reply_ct);
    init_decoded_content (reply_ct, infile);

    if (extract_headers (reply_ct, reply_file, &reply_fp) == NOTOK) {
        free (reply_file);
        inform("failed to extract headers from convert output in %s, "
	    "continuing...", reply_file);
        return;
    }

    /* For text content only, see if it is 8-bit text. */
    if (reply_ct->c_type == CT_TEXT) {
        int fd;

        if ((fd = open (reply_file, O_RDONLY)) == NOTOK  ||
            scan_input (fd, &eightbit) == NOTOK) {
            free (reply_file);
            inform("failed to read %s, continuing...", reply_file);
            return;
        } 
        (void) close (fd);
    }

    /* This sets reply_ct->c_ctparams, and reply_ct->c_termproc if the
       charset can't be handled natively. */
    for (s2i = str2cts; s2i->si_key; s2i++) {
        if (strcasecmp(reply_ct->c_ctinfo.ci_type, s2i->si_key) == 0) {
            break;
        }
    }

    if ((reply_ct->c_ctinitfnx = s2i->si_init)) {
        (*reply_ct->c_ctinitfnx)(reply_ct);
    }

    if ((cp =
         get_param (reply_ct->c_ctinfo.ci_first_pm, "charset", '?', 1))) {
        /* The reply Content-Type had the charset. */
        charset = cp;
    } else {
        set_charset (reply_ct, -1);
        charset = get_param (reply_ct->c_ctinfo.ci_first_pm, "charset", '?', 1);
        if (reply_ct->c_reqencoding == CE_UNKNOWN  &&
            reply_ct->c_type == CT_TEXT) {
            /* Assume that 8bit is sufficient (for text).  In other words,
               don't allow it to be encoded as quoted printable if lines
               are too long.  This also sidesteps the check for whether
               it needs to be encoded as binary; instead, it relies on
               the applicable mhbuild-convert-text directive to ensure
               that the resultant text is not binary. */
            reply_ct->c_reqencoding = eightbit  ?  CE_8BIT  :  CE_7BIT;
        }
    }

    /* Concatenate text/plain parts. */
    if (reply_ct->c_type == CT_TEXT  &&
        reply_ct->c_subtype == TEXT_PLAIN) {
        if (! *text_plain_ct  &&  m->mp_parts  &&  m->mp_parts->mp_part  &&
            m->mp_parts->mp_part->c_type == CT_TEXT  &&
            m->mp_parts->mp_part->c_subtype == TEXT_PLAIN) {
            *text_plain_ct = m->mp_parts->mp_part;
            /* Make sure that the charset is set in the text/plain
               part. */
            set_charset (*text_plain_ct, -1);
            if ((*text_plain_ct)->c_reqencoding == CE_UNKNOWN) {
                /* Assume that 8bit is sufficient (for text).  In other words,
                   don't allow it to be encoded as quoted printable if lines
                   are too long.  This also sidesteps the check for whether
                   it needs to be encoded as binary; instead, it relies on
                   the applicable mhbuild-convert-text directive to ensure
                   that the resultant text is not binary. */
                (*text_plain_ct)->c_reqencoding =
                    eightbit  ?  CE_8BIT  :  CE_7BIT;
            }
        }

        if (*text_plain_ct) {
            /* Only concatenate if the charsets are identical. */
            char *text_plain_ct_charset =
                get_param ((*text_plain_ct)->c_ctinfo.ci_first_pm, "charset",
                           '?', 1);

            if (strcasecmp (text_plain_ct_charset, charset) == 0) {
                /* Append this text/plain reply to the first one.
                   If there's a problem anywhere along the way,
                   instead attach it is a separate part. */
                int text_plain_reply =
                    open ((*text_plain_ct)->c_cefile.ce_file,
                          O_WRONLY | O_APPEND);
                int addl_reply = open (reply_file, O_RDONLY);

                if (text_plain_reply != NOTOK  &&  addl_reply != NOTOK) {
                    /* Insert blank line before each addl part. */
                    /* It would be nice not to do this for the first one. */
                    if (write (text_plain_reply, "\n", 1) == 1) {
                        /* Copy the text from the new reply and
                           then free its Content struct. */
                        cpydata (addl_reply, text_plain_reply,
                                 (*text_plain_ct)->c_cefile.ce_file,
                                 reply_file);
                        if (close (text_plain_reply) == OK  &&
                            close (addl_reply) == OK) {
                            /* If appended text needed 8-bit but first text didn't,
                               propagate the 8-bit indication. */
                            if ((*text_plain_ct)->c_reqencoding == CE_7BIT  &&
                                reply_ct->c_reqencoding == CE_8BIT) {
                                (*text_plain_ct)->c_reqencoding = CE_8BIT;
                            }

                            if (reply_fp) { fclose (reply_fp); }
                            free (reply_file);
                            free_content (reply_ct);
                            return;
                        }
                    }
                }
            }
        } else {
            *text_plain_ct = reply_ct;
        }
    }

    reply_ct->c_cefile.ce_file = reply_file;
    reply_ct->c_cefile.ce_fp = reply_fp;
    reply_ct->c_cefile.ce_unlink = 1;

    /* Attach the new part to the parent multipart/mixed, "m". */
    NEW0(part);
    part->mp_part = reply_ct;
    if (m->mp_parts) {
        struct part *p;

        for (p = m->mp_parts; p && p->mp_next; p = p->mp_next) { continue; }
        p->mp_next = part;
    } else {
        m->mp_parts = part;
    }
}


/* Extract any Content-Type header from beginning of convert output. */
int
extract_headers (CT ct, char *reply_file, FILE **reply_fp) {
    char *buffer = NULL, *cp, *end_of_header;
    int found_header = 0;
    struct stat statbuf;

    /* Read the convert reply from the file to memory. */
    if (stat (reply_file, &statbuf) == NOTOK) {
        admonish (reply_file, "failed to stat");
        goto failed_to_extract_ct;
    }

    buffer = mh_xmalloc (statbuf.st_size + 1);

    if ((*reply_fp = fopen (reply_file, "r+")) == NULL  ||
        fread (buffer, 1, (size_t) statbuf.st_size, *reply_fp) <
            (size_t) statbuf.st_size) {
        admonish (reply_file, "failed to read");
        goto failed_to_extract_ct;
    }
    buffer[statbuf.st_size] = '\0';

    /* Look for a header in the convert reply. */
    if (strncasecmp (buffer, TYPE_FIELD, strlen (TYPE_FIELD)) == 0  &&
        buffer[strlen (TYPE_FIELD)] == ':') {
        if ((end_of_header = strstr (buffer, "\r\n\r\n"))) {
            end_of_header += 2;
            found_header = 1;
        } else if ((end_of_header = strstr (buffer, "\n\n"))) {
            ++end_of_header;
            found_header = 1;
        }
    }

    if (found_header) {
        CT tmp_ct;
        char *tmp_file;
        FILE *tmp_f;
        size_t n;

        /* Truncate buffer to just the C-T. */
        *end_of_header = '\0';
        n = strlen (buffer);

        if (get_ctinfo (buffer + 14, ct, 0) != OK) {
            inform("unable to get content info for reply, continuing...");
            goto failed_to_extract_ct;
        }

        /* Hack.  Use parse_mime() to detect the type/subtype of the
           reply, which we'll use below. */
        tmp_file = getcpy (m_mktemp2 (NULL, invo_name, NULL, NULL));
        if ((tmp_f = fopen (tmp_file, "w"))  &&
            fwrite (buffer, 1, n, tmp_f) == n) {
            fclose (tmp_f);
        } else {
            goto failed_to_extract_ct;
        }
        tmp_ct = parse_mime (tmp_file);

        if (tmp_ct) {
            /* The type and subtype were detected from the reply
               using parse_mime() above. */
            ct->c_type = tmp_ct->c_type;
            ct->c_subtype = tmp_ct->c_subtype;
            free_content (tmp_ct);
        }

        free (tmp_file);

        /* Rewrite the content without the header. */
        cp = end_of_header + 1;
        rewind (*reply_fp);

        if (fwrite (cp, 1, statbuf.st_size - (cp - buffer), *reply_fp) <
            (size_t) (statbuf.st_size - (cp - buffer))) {
            admonish (reply_file, "failed to write");
            goto failed_to_extract_ct;
        }

        if (ftruncate (fileno (*reply_fp), statbuf.st_size - (cp - buffer)) !=
            0) {
            advise (reply_file, "ftruncate");
            goto failed_to_extract_ct;
        }
    } else {
        /* No header section, assume the reply is text/plain. */
        ct->c_type = CT_TEXT;
        ct->c_subtype = TEXT_PLAIN;
        if (get_ctinfo ("text/plain", ct, 0) == NOTOK) {
            /* This never should fail, but just in case. */
            adios (NULL, "unable to get content info for reply");
        }
    }

    /* free_encoding() will close reply_fp, which is passed through
       ct->c_cefile.ce_fp. */
    free (buffer);
    return OK;

failed_to_extract_ct:
    if (*reply_fp) { fclose (*reply_fp); }
    free (buffer);
    return NOTOK;
}
