
/*
 * mhbuildsbr.c -- routines to expand/translate MIME composition files
 *
 * $Id$
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

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef TM_IN_SYS_TIME
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif


extern int debugsw;
extern int verbosw;

extern int ebcdicsw;
extern int listsw;
extern int rfc934sw;

extern int endian;	/* mhmisc.c */

/* cache policies */
extern int rcachesw;	/* mhcachesbr.c */
extern int wcachesw;	/* mhcachesbr.c */

int checksw = 0;	/* Add Content-MD5 field */

/*
 * Directory to place tmp files.  This must
 * be set before these routines are called.
 */
char *tmp;

pid_t xpid = 0;

static char prefix[] = "----- =_aaaaaaaaaa";

/*
 * Structure for mapping types to their internal flags
 */
struct k2v {
    char *kv_key;
    int	  kv_value;
};

/*
 * Structures for TEXT messages
 */
static struct k2v SubText[] = {
    { "plain",    TEXT_PLAIN },
    { "richtext", TEXT_RICHTEXT },  /* defined in RFC-1341    */
    { "enriched", TEXT_ENRICHED },  /* defined in RFC-1896    */
    { NULL,       TEXT_UNKNOWN }    /* this one must be last! */
};

static struct k2v Charset[] = {
    { "us-ascii",   CHARSET_USASCII },
    { "iso-8859-1", CHARSET_LATIN },
    { NULL,         CHARSET_UNKNOWN }  /* this one must be last! */
};

/*
 * Structures for MULTIPART messages
 */
static struct k2v SubMultiPart[] = {
    { "mixed",       MULTI_MIXED },
    { "alternative", MULTI_ALTERNATE },
    { "digest",      MULTI_DIGEST },
    { "parallel",    MULTI_PARALLEL },
    { NULL,          MULTI_UNKNOWN }    /* this one must be last! */
};

/*
 * Structures for MESSAGE messages
 */
static struct k2v SubMessage[] = {
    { "rfc822",        MESSAGE_RFC822 },
    { "partial",       MESSAGE_PARTIAL },
    { "external-body", MESSAGE_EXTERNAL },
    { NULL,            MESSAGE_UNKNOWN }	/* this one must be last! */
};

/*
 * Structure for APPLICATION messages
 */
static struct k2v SubApplication[] = {
    { "octet-stream", APPLICATION_OCTETS },
    { "postscript",   APPLICATION_POSTSCRIPT },
    { NULL,           APPLICATION_UNKNOWN }	/* this one must be last! */
};


/* mhmisc.c */
int make_intermediates (char *);
void content_error (char *, CT, char *, ...);

/* mhcachesbr.c */
int find_cache (CT, int, int *, char *, char *, int);

/* ftpsbr.c */
int ftp_get (char *, char *, char *, char *, char *, char *, int, int);

/* mhfree.c */
void free_content (CT);
void free_ctinfo (CT);
void free_encoding (CT, int);

/*
 * prototypes
 */
CT build_mime (char *);
int pidcheck (int);

/*
 * static prototypes
 */
static CT get_content (FILE *, char *, int);
static int add_header (CT, char *, char *);
static int get_ctinfo (char *, CT, int);
static int get_comment (CT, char **, int);
static int InitGeneric (CT);
static int InitText (CT);
static int InitMultiPart (CT);
static void reverse_parts (CT);
static int InitMessage (CT);
static int params_external (CT, int);
static int InitApplication (CT);
static int init_decoded_content (CT);
static int init_encoding (CT, OpenCEFunc);
static void close_encoding (CT);
static unsigned long size_encoding (CT);
static int InitBase64 (CT);
static int openBase64 (CT, char **);
static int InitQuoted (CT);
static int openQuoted (CT, char **);
static int Init7Bit (CT);
static int open7Bit (CT, char **);
static int openExternal (CT, CT, CE, char **, int *);
static int InitFile (CT);
static int openFile (CT, char **);
static int InitFTP (CT);
static int openFTP (CT, char **);
static int InitMail (CT);
static int openMail (CT, char **);
static char *fgetstr (char *, int, FILE *);
static int user_content (FILE *, char *, char *, CT *);
static void set_id (CT, int);
static int compose_content (CT);
static int scan_content (CT);
static int build_headers (CT);
static char *calculate_digest (CT, int);
static int readDigest (CT, char *);

/*
 * Structures for mapping (content) types to
 * the functions to handle them.
 */
struct str2init {
    char *si_key;
    int	  si_val;
    InitFunc si_init;
};

static struct str2init str2cts[] = {
    { "application", CT_APPLICATION, InitApplication },
    { "audio",	     CT_AUDIO,	     InitGeneric },
    { "image",	     CT_IMAGE,	     InitGeneric },
    { "message",     CT_MESSAGE,     InitMessage },
    { "multipart",   CT_MULTIPART,   InitMultiPart },
    { "text",	     CT_TEXT,	     InitText },
    { "video",	     CT_VIDEO,	     InitGeneric },
    { NULL,	     CT_EXTENSION,   NULL },  /* these two must be last! */
    { NULL,	     CT_UNKNOWN,     NULL },
};

static struct str2init str2ces[] = {
    { "base64",		  CE_BASE64,	InitBase64 },
    { "quoted-printable", CE_QUOTED,	InitQuoted },
    { "8bit",		  CE_8BIT,	Init7Bit },
    { "7bit",		  CE_7BIT,	Init7Bit },
    { "binary",		  CE_BINARY,	NULL },
    { NULL,		  CE_EXTENSION,	NULL },	 /* these two must be last! */
    { NULL,		  CE_UNKNOWN,	NULL },
};

/*
 * NOTE WELL: si_key MUST NOT have value of NOTOK
 *
 * si_key is 1 if access method is anonymous.
 */
static struct str2init str2methods[] = {
    { "afs",         1,	InitFile },
    { "anon-ftp",    1,	InitFTP },
    { "ftp",         0,	InitFTP },
    { "local-file",  0,	InitFile },
    { "mail-server", 0,	InitMail },
    { NULL,          0, NULL }
};


int
pidcheck (int status)
{
    if ((status & 0xff00) == 0xff00 || (status & 0x007f) != SIGQUIT)
	return status;

    fflush (stdout);
    fflush (stderr);
    return done (1);
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
build_mime (char *infile)
{
    int	compnum, state;
    char buf[BUFSIZ], name[NAMESZ];
    char *cp, *np, *vp;
    struct multipart *m;
    struct part **pp;
    CT ct;
    FILE *in;

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
	    if (!strcasecmp (name, VRSN_FIELD))
		adios (NULL, "draft shouldn't contain %s: field", VRSN_FIELD);

	    /* abort if draft has Content-Transfer-Encoding header field */
	    if (!strcasecmp (name, ENCODING_FIELD))
		adios (NULL, "draft shouldn't contain %s: field", ENCODING_FIELD);

	    /* ignore any Content-Type fields in the header */
	    if (!strcasecmp (name, TYPE_FIELD)) {
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
 * Main routine for reading/parsing the headers
 * of a message content.
 *
 * toplevel =  1   # we are at the top level of the message
 * toplevel =  0   # we are inside message type or multipart type
 *                 # other than multipart/digest
 * toplevel = -1   # we are inside multipart/digest
 */

static CT
get_content (FILE *in, char *file, int toplevel)
{
    int compnum, state;
    char buf[BUFSIZ], name[NAMESZ];
    CT ct;

    if (!(ct = (CT) calloc (1, sizeof(*ct))))
	adios (NULL, "out of memory");

    ct->c_fp = in;
    ct->c_file = add (file, NULL);
    ct->c_begin = ftell (ct->c_fp) + 1;

    /*
     * Read the content headers
     */
    for (compnum = 1, state = FLD;;) {
	switch (state = m_getfld (state, name, buf, sizeof(buf), in)) {
	case FLD:
	case FLDPLUS:
	case FLDEOF:
	    compnum++;

	    /* Get MIME-Version field */
	    if (!strcasecmp (name, VRSN_FIELD)) {
		int ucmp;
		char c, *cp, *dp;

		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		    cp = add (buf, cp);
		}

		if (ct->c_vrsn) {
		    advise (NULL, "message %s has multiple %s: fields (%s)",
			    ct->c_file, VRSN_FIELD, dp = trimcpy (cp));
		    free (dp);
		    free (cp);
		    goto out;
		}

		ct->c_vrsn = cp;
		while (isspace (*cp))
		    cp++;
		for (dp = strchr(cp, '\n'); dp; dp = strchr(dp, '\n'))
		    *dp++ = ' ';
		for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
		    if (!isspace (*dp))
			break;
		*++dp = '\0';
		if (debugsw)
		    fprintf (stderr, "%s: %s\n", VRSN_FIELD, cp);

		if (*cp == '(' && get_comment (ct, &cp, 0) == NOTOK)
		    goto out;

		for (dp = cp; istoken (*dp); dp++)
		    continue;
		c = *dp, *dp = '\0';
		ucmp = !strcasecmp (cp, VRSN_VALUE);
		*dp = c;
		if (!ucmp)
		    admonish (NULL,
			      "message %s has unknown value for %s: field (%s)",
			      ct->c_file, VRSN_FIELD, cp);
		goto got_header;
	    }

	    /* Get Content-Type field */
	    if (!strcasecmp (name, TYPE_FIELD)) {
		char *cp;
		struct str2init *s2i;
		CI ci = &ct->c_ctinfo;

		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		    cp = add (buf, cp);
		}

		/* Check if we've already seen a Content-Type header */
		if (ct->c_ctline) {
		    char *dp = trimcpy (cp);

		    advise (NULL, "message %s has multiple %s: fields (%s)",
			    ct->c_file, TYPE_FIELD, dp);
		    free (dp);
		    free (cp);
		    goto out;
		}

		/* Parse the Content-Type field */
		if (get_ctinfo (cp, ct, 0) == NOTOK)
		    goto out;

		/*
		 * Set the Init function and the internal
		 * flag for this content type.
		 */
		for (s2i = str2cts; s2i->si_key; s2i++)
		    if (!strcasecmp (ci->ci_type, s2i->si_key))
			break;
		if (!s2i->si_key && !uprf (ci->ci_type, "X-"))
		    s2i++;
		ct->c_type = s2i->si_val;
		ct->c_ctinitfnx = s2i->si_init;
		goto got_header;
	    }

	    /* Get Content-Transfer-Encoding field */
	    if (!strcasecmp (name, ENCODING_FIELD)) {
		char *cp, *dp;
		char c;
		struct str2init *s2i;

		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		    cp = add (buf, cp);
		}

		/*
		 * Check if we've already seen the
		 * Content-Transfer-Encoding field
		 */
		if (ct->c_celine) {
		    advise (NULL, "message %s has multiple %s: fields (%s)",
			    ct->c_file, ENCODING_FIELD, dp = trimcpy (cp));
		    free (dp);
		    free (cp);
		    goto out;
		}

		ct->c_celine = cp;	/* Save copy of this field */
		while (isspace (*cp))
		    cp++;
		for (dp = cp; istoken (*dp); dp++)
		    continue;
		c = *dp;
		*dp = '\0';

		/*
		 * Find the internal flag and Init function
		 * for this transfer encoding.
		 */
		for (s2i = str2ces; s2i->si_key; s2i++)
		    if (!strcasecmp (cp, s2i->si_key))
			break;
		if (!s2i->si_key && !uprf (cp, "X-"))
		    s2i++;
		*dp = c;
		ct->c_encoding = s2i->si_val;

		/* Call the Init function for this encoding */
		if (s2i->si_init && (*s2i->si_init) (ct) == NOTOK)
		    goto out;
		goto got_header;
	    }

	    /* Get Content-ID field */
	    if (!strcasecmp (name, ID_FIELD)) {
		ct->c_id = add (buf, ct->c_id);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		    ct->c_id = add (buf, ct->c_id);
		}
		goto got_header;
	    }

	    /* Get Content-Description field */
	    if (!strcasecmp (name, DESCR_FIELD)) {
		ct->c_descr = add (buf, ct->c_descr);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		    ct->c_descr = add (buf, ct->c_descr);
		}
		goto got_header;
	    }

	    /* Get Content-MD5 field */
	    if (!strcasecmp (name, MD5_FIELD)) {
		char *cp, *dp, *ep;

		cp = add (buf, NULL);
		while (state == FLDPLUS) {
		    state = m_getfld (state, name, buf, sizeof(buf), in);
		    cp = add (buf, cp);
		}

		if (!checksw) {
		    free (cp);
		    goto got_header;
		}

		if (ct->c_digested) {
		    advise (NULL, "message %s has multiple %s: fields (%s)",
			    ct->c_file, MD5_FIELD, dp = trimcpy (cp));
		    free (dp);
		    free (cp);
		    goto out;
		}

		ep = cp;
		while (isspace (*cp))
		    cp++;
		for (dp = strchr(cp, '\n'); dp; dp = strchr(dp, '\n'))
		    *dp++ = ' ';
		for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
		    if (!isspace (*dp))
			break;
		*++dp = '\0';
		if (debugsw)
		    fprintf (stderr, "%s: %s\n", MD5_FIELD, cp);

		if (*cp == '(' && get_comment (ct, &cp, 0) == NOTOK) {
		    free (ep);
		    goto out;
		}

		for (dp = cp; *dp && !isspace (*dp); dp++)
		    continue;
		*dp = '\0';

		readDigest (ct, cp);
		free (ep);
		ct->c_digested++;
		goto got_header;
	    }

#if 0
	    if (uprf (name, XXX_FIELD_PRF))
		advise (NULL, "unknown field (%s) in message %s",
			name, ct->c_file);
	    /* and fall... */
#endif

	    while (state == FLDPLUS)
		state = m_getfld (state, name, buf, sizeof(buf), in);

got_header:
	    if (state != FLDEOF) {
		ct->c_begin = ftell (in) + 1;
		continue;
	    }
	    /* else fall... */

	case BODY:
	case BODYEOF:
	    ct->c_begin = ftell (in) - strlen (buf);
	    break;

	case FILEEOF:
	    ct->c_begin = ftell (in);
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
     * Check if we saw a Content-Type field.
     * If not, then assign a default value for
     * it, and the Init function.
     */
    if (!ct->c_ctline) {
	/*
	 * If we are inside a multipart/digest message,
	 * so default type is message/rfc822
	 */
	if (toplevel < 0) {
	    if (get_ctinfo ("message/rfc822", ct, 0) == NOTOK)
		goto out;
	    ct->c_type = CT_MESSAGE;
	    ct->c_ctinitfnx = InitMessage;
	} else {
	    /*
	     * Else default type is text/plain
	     */
	    if (get_ctinfo ("text/plain", ct, 0) == NOTOK)
		goto out;
	    ct->c_type = CT_TEXT;
	    ct->c_ctinitfnx = InitText;
	}
    }

    /* Use default Transfer-Encoding, if necessary */
    if (!ct->c_celine) {
	ct->c_encoding = CE_7BIT;
	Init7Bit (ct);
    }

    return ct;

out:
    free_content (ct);
    return NULL;
}


/*
 * small routine to add header field to list
 */

static int
add_header (CT ct, char *name, char *value)
{
    HF hp;

    /* allocate header field structure */
    if (!(hp = malloc (sizeof(*hp))))
	adios (NULL, "out of memory");

    /* link data into header structure */
    hp->name = name;
    hp->value = value;
    hp->next = NULL;

    /* link header structure into the list */
    if (ct->c_first_hf == NULL) {
	ct->c_first_hf = hp;		/* this is the first */
	ct->c_last_hf = hp;
    } else {
	ct->c_last_hf->next = hp;	/* add it to the end */
	ct->c_last_hf = hp;
    }

    return 0;
}


/*
 * Used to parse both:
 *   1) Content-Type line
 *   2) composition directives
 *
 * and fills in the information of the CTinfo structure.
 */

static int
get_ctinfo (char *cp, CT ct, int magic)
{
    int	i;
    char *dp, **ap, **ep;
    char c;
    CI ci;

    ci = &ct->c_ctinfo;
    i = strlen (invo_name) + 2;

    /* store copy of Content-Type line */
    cp = ct->c_ctline = add (cp, NULL);

    while (isspace (*cp))	/* trim leading spaces */
	cp++;

    /* change newlines to spaces */
    for (dp = strchr(cp, '\n'); dp; dp = strchr(dp, '\n'))
	*dp++ = ' ';

    /* trim trailing spaces */
    for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
	if (!isspace (*dp))
	    break;
    *++dp = '\0';

    if (debugsw)
	fprintf (stderr, "%s: %s\n", TYPE_FIELD, cp);

    if (*cp == '(' && get_comment (ct, &cp, 1) == NOTOK)
	return NOTOK;

    for (dp = cp; istoken (*dp); dp++)
	continue;
    c = *dp, *dp = '\0';
    ci->ci_type = add (cp, NULL);	/* store content type */
    *dp = c, cp = dp;

    if (!*ci->ci_type) {
	advise (NULL, "invalid %s: field in message %s (empty type)", 
		TYPE_FIELD, ct->c_file);
	return NOTOK;
    }

    /* down case the content type string */
    for (dp = ci->ci_type; *dp; dp++)
	if (isalpha(*dp) && isupper (*dp))
	    *dp = tolower (*dp);

    while (isspace (*cp))
	cp++;

    if (*cp == '(' && get_comment (ct, &cp, 1) == NOTOK)
	return NOTOK;

    if (*cp != '/') {
	if (!magic)
	    ci->ci_subtype = add ("", NULL);
	goto magic_skip;
    }

    cp++;
    while (isspace (*cp))
	cp++;

    if (*cp == '(' && get_comment (ct, &cp, 1) == NOTOK)
	return NOTOK;

    for (dp = cp; istoken (*dp); dp++)
	continue;
    c = *dp, *dp = '\0';
    ci->ci_subtype = add (cp, NULL);	/* store the content subtype */
    *dp = c, cp = dp;

    if (!*ci->ci_subtype) {
	advise (NULL,
		"invalid %s: field in message %s (empty subtype for \"%s\")",
		TYPE_FIELD, ct->c_file, ci->ci_type);
	return NOTOK;
    }

    /* down case the content subtype string */
    for (dp = ci->ci_subtype; *dp; dp++)
	if (isalpha(*dp) && isupper (*dp))
	    *dp = tolower (*dp);

magic_skip:
    while (isspace (*cp))
	cp++;

    if (*cp == '(' && get_comment (ct, &cp, 1) == NOTOK)
	return NOTOK;

    /*
     * Parse attribute/value pairs given with Content-Type
     */
    ep = (ap = ci->ci_attrs) + NPARMS;
    while (*cp == ';') {
	char *vp, *up;

	if (ap >= ep) {
	    advise (NULL,
		    "too many parameters in message %s's %s: field (%d max)",
		    ct->c_file, TYPE_FIELD, NPARMS);
	    return NOTOK;
	}

	cp++;
	while (isspace (*cp))
	    cp++;

	if (*cp == '(' && get_comment (ct, &cp, 1) == NOTOK)
	    return NOTOK;

	if (*cp == 0) {
	    advise (NULL,
		    "extraneous trailing ';' in message %s's %s: parameter list",
		    ct->c_file, TYPE_FIELD);
	    return OK;
	}

	/* down case the attribute name */
	for (dp = cp; istoken (*dp); dp++)
	    if (isalpha(*dp) && isupper (*dp))
		*dp = tolower (*dp);

	for (up = dp; isspace (*dp); )
	    dp++;
	if (dp == cp || *dp != '=') {
	    advise (NULL,
		    "invalid parameter in message %s's %s: field\n%*.*sparameter %s (error detected at offset %d)",
		    ct->c_file, TYPE_FIELD, i, i, "", cp, dp - cp);
	    return NOTOK;
	}

	vp = (*ap = add (cp, NULL)) + (up - cp);
	*vp = '\0';
	for (dp++; isspace (*dp); )
	    dp++;

	/* now add the attribute value */
	ci->ci_values[ap - ci->ci_attrs] = vp = *ap + (dp - cp);

	if (*dp == '"') {
	    for (cp = ++dp, dp = vp;;) {
		switch (c = *cp++) {
		    case '\0':
bad_quote:
		        advise (NULL,
				"invalid quoted-string in message %s's %s: field\n%*.*s(parameter %s)",
				ct->c_file, TYPE_FIELD, i, i, "", *ap);
			return NOTOK;

		    case '\\':
			*dp++ = c;
			if ((c = *cp++) == '\0')
			    goto bad_quote;
			/* else fall... */

		    default:
    			*dp++ = c;
    			continue;

		    case '"':
			*dp = '\0';
			break;
		}
		break;
	    }
	} else {
	    for (cp = dp, dp = vp; istoken (*cp); cp++, dp++)
		continue;
	    *dp = '\0';
	}
	if (!*vp) {
	    advise (NULL,
		    "invalid parameter in message %s's %s: field\n%*.*s(parameter %s)",
		    ct->c_file, TYPE_FIELD, i, i, "", *ap);
	    return NOTOK;
	}
	ap++;

	while (isspace (*cp))
	    cp++;

	if (*cp == '(' && get_comment (ct, &cp, 1) == NOTOK)
	    return NOTOK;
    }

    /*
     * Get any <Content-Id> given in buffer
     */
    if (magic && *cp == '<') {
	if (ct->c_id) {
	    free (ct->c_id);
	    ct->c_id = NULL;
	}
	if (!(dp = strchr(ct->c_id = ++cp, '>'))) {
	    advise (NULL, "invalid ID in message %s", ct->c_file);
	    return NOTOK;
	}
	c = *dp;
	*dp = '\0';
	if (*ct->c_id)
	    ct->c_id = concat ("<", ct->c_id, ">\n", NULL);
	else
	    ct->c_id = NULL;
	*dp++ = c;
	cp = dp;

	while (isspace (*cp))
	    cp++;
    }

    /*
     * Get any [Content-Description] given in buffer.
     */
    if (magic && *cp == '[') {
	ct->c_descr = ++cp;
	for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
	    if (*dp == ']')
		break;
	if (dp < cp) {
	    advise (NULL, "invalid description in message %s", ct->c_file);
	    ct->c_descr = NULL;
	    return NOTOK;
	}
	
	c = *dp;
	*dp = '\0';
	if (*ct->c_descr)
	    ct->c_descr = concat (ct->c_descr, "\n", NULL);
	else
	    ct->c_descr = NULL;
	*dp++ = c;
	cp = dp;

	while (isspace (*cp))
	    cp++;
    }

    /*
     * Check if anything is left over
     */
    if (*cp) {
	if (magic)
	    ci->ci_magic = add (cp, NULL);
	else
	    advise (NULL,
		    "extraneous information in message %s's %s: field\n%*.*s(%s)",
		ct->c_file, TYPE_FIELD, i, i, "", cp);
    }

    return OK;
}


static int
get_comment (CT ct, char **ap, int istype)
{
    int i;
    char *bp, *cp;
    char c, buffer[BUFSIZ], *dp;
    CI ci;

    ci = &ct->c_ctinfo;
    cp = *ap;
    bp = buffer;
    cp++;

    for (i = 0;;) {
	switch (c = *cp++) {
	case '\0':
invalid:
	advise (NULL, "invalid comment in message %s's %s: field",
		ct->c_file, istype ? TYPE_FIELD : VRSN_FIELD);
	return NOTOK;

	case '\\':
	    *bp++ = c;
	    if ((c = *cp++) == '\0')
		goto invalid;
	    *bp++ = c;
	    continue;

	case '(':
	    i++;
	    /* and fall... */
	default:
	    *bp++ = c;
	    continue;

	case ')':
	    if (--i < 0)
		break;
	    *bp++ = c;
	    continue;
	}
	break;
    }
    *bp = '\0';

    if (istype) {
	if ((dp = ci->ci_comment)) {
	    ci->ci_comment = concat (dp, " ", buffer, NULL);
	    free (dp);
	} else {
	    ci->ci_comment = add (buffer, NULL);
	}
    }

    while (isspace (*cp))
	cp++;

    *ap = cp;
    return OK;
}


/*
 * CONTENTS
 *
 * Handles content types audio, image, and video.
 * There's not much to do right here.
 */

static int
InitGeneric (CT ct)
{
    return OK;		/* not much to do here */
}


/*
 * TEXT
 */

static int
InitText (CT ct)
{
    char **ap, **ep;
    struct k2v *kv;
    struct text *t;
    CI ci = &ct->c_ctinfo;

    /* check for missing subtype */
    if (!*ci->ci_subtype)
	ci->ci_subtype = add ("plain", ci->ci_subtype);

    /* match subtype */
    for (kv = SubText; kv->kv_key; kv++)
	if (!strcasecmp (ci->ci_subtype, kv->kv_key))
	    break;
    ct->c_subtype = kv->kv_value;

    /* allocate text character set structure */
    if ((t = (struct text *) calloc (1, sizeof(*t))) == NULL)
	adios (NULL, "out of memory");
    ct->c_ctparams = (void *) t;

    /* initially mark character set as unspecified */
    t->tx_charset = CHARSET_UNSPECIFIED;

    /* scan for charset parameter */
    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++)
	if (!strcasecmp (*ap, "charset"))
	    break;

    /* check if content specified a character set */
    if (*ap) {
	/* match character set or set to CHARSET_UNKNOWN */
	for (kv = Charset; kv->kv_key; kv++)
	    if (!strcasecmp (*ep, kv->kv_key))
		break;
	t->tx_charset = kv->kv_value;
    }

    return OK;
}


/*
 * MULTIPART
 */

static int
InitMultiPart (CT ct)
{
    int	inout;
    long last, pos;
    char *cp, *dp, **ap, **ep;
    char *bp, buffer[BUFSIZ];
    struct multipart *m;
    struct k2v *kv;
    struct part *part, **next;
    CI ci = &ct->c_ctinfo;
    CT p;
    FILE *fp;

    /*
     * The encoding for multipart messages must be either
     * 7bit, 8bit, or binary (per RFC2045).
     */
    if (ct->c_encoding != CE_7BIT && ct->c_encoding != CE_8BIT
	&& ct->c_encoding != CE_BINARY) {
	admonish (NULL,
		  "\"%s/%s\" type in message %s must be encoded in 7bit, 8bit, or binary",
		  ci->ci_type, ci->ci_subtype, ct->c_file);
	return NOTOK;
    }

    /* match subtype */
    for (kv = SubMultiPart; kv->kv_key; kv++)
	if (!strcasecmp (ci->ci_subtype, kv->kv_key))
	    break;
    ct->c_subtype = kv->kv_value;

    /*
     * Check for "boundary" parameter, which is
     * required for multipart messages.
     */
    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
	if (!strcasecmp (*ap, "boundary")) {
	    bp = *ep;
	    break;
	}
    }

    /* complain if boundary parameter is missing */
    if (!*ap) {
	advise (NULL,
		"a \"boundary\" parameter is mandatory for \"%s/%s\" type in message %s's %s: field",
		ci->ci_type, ci->ci_subtype, ct->c_file, TYPE_FIELD);
	return NOTOK;
    }

    /* allocate primary structure for multipart info */
    if ((m = (struct multipart *) calloc (1, sizeof(*m))) == NULL)
	adios (NULL, "out of memory");
    ct->c_ctparams = (void *) m;

    /* check if boundary parameter contains only whitespace characters */
    for (cp = bp; isspace (*cp); cp++)
	continue;
    if (!*cp) {
	advise (NULL, "invalid \"boundary\" parameter for \"%s/%s\" type in message %s's %s: field",
		ci->ci_type, ci->ci_subtype, ct->c_file, TYPE_FIELD);
	return NOTOK;
    }

    /* remove trailing whitespace from boundary parameter */
    for (cp = bp, dp = cp + strlen (cp) - 1; dp > cp; dp--)
	if (!isspace (*dp))
	    break;
    *++dp = '\0';

    /* record boundary separators */
    m->mp_start = concat (bp, "\n", NULL);
    m->mp_stop = concat (bp, "--\n", NULL);

    if (!ct->c_fp && (ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
	advise (ct->c_file, "unable to open for reading");
	return NOTOK;
    }

    fseek (fp = ct->c_fp, pos = ct->c_begin, SEEK_SET);
    last = ct->c_end;
    next = &m->mp_parts;
    part = NULL;
    inout = 1;

    while (fgets (buffer, sizeof(buffer) - 1, fp)) {
	if (pos > last)
	    break;

	pos += strlen (buffer);
	if (buffer[0] != '-' || buffer[1] != '-')
	    continue;
	if (inout) {
	    if (strcmp (buffer + 2, m->mp_start))
		continue;
next_part:
	    if ((part = (struct part *) calloc (1, sizeof(*part))) == NULL)
		adios (NULL, "out of memory");
	    *next = part;
	    next = &part->mp_next;

	    if (!(p = get_content (fp, ct->c_file,
		rfc934sw && ct->c_subtype == MULTI_DIGEST ? -1 : 0))) {
		fclose (ct->c_fp);
		ct->c_fp = NULL;
		return NOTOK;
	    }
	    p->c_fp = NULL;
	    part->mp_part = p;
	    pos = p->c_begin;
	    fseek (fp, pos, SEEK_SET);
	    inout = 0;
	} else {
	    if (strcmp (buffer + 2, m->mp_start) == 0) {
		inout = 1;
end_part:
		p = part->mp_part;
		p->c_end = ftell(fp) - (strlen(buffer) + 1);
		if (p->c_end < p->c_begin)
		    p->c_begin = p->c_end;
		if (inout)
		    goto next_part;
		goto last_part;
	    } else {
		if (strcmp (buffer + 2, m->mp_stop) == 0)
		    goto end_part;
	    }
	}
    }

    advise (NULL, "bogus multipart content in message %s", ct->c_file);
    if (!inout && part) {
	p = part->mp_part;
	p->c_end = ct->c_end;

	if (p->c_begin >= p->c_end) {
	    for (next = &m->mp_parts; *next != part;
		     next = &((*next)->mp_next))
		continue;
	    *next = NULL;
	    free_content (p);
	    free ((char *) part);
	}
    }

last_part:
    /* reverse the order of the parts for multipart/alternative */
    if (ct->c_subtype == MULTI_ALTERNATE)
	reverse_parts (ct);

    /*
     * label all subparts with part number, and
     * then initialize the content of the subpart.
     */
    {
	int partnum;
	char *pp;
	char partnam[BUFSIZ];

	if (ct->c_partno) {
	    snprintf (partnam, sizeof(partnam), "%s.", ct->c_partno);
	    pp = partnam + strlen (partnam);
	} else {
	    pp = partnam;
	}

	for (part = m->mp_parts, partnum = 1; part;
	         part = part->mp_next, partnum++) {
	    p = part->mp_part;

	    sprintf (pp, "%d", partnum);
	    p->c_partno = add (partnam, NULL);

	    /* initialize the content of the subparts */
	    if (p->c_ctinitfnx && (*p->c_ctinitfnx) (p) == NOTOK) {
		fclose (ct->c_fp);
		ct->c_fp = NULL;
		return NOTOK;
	    }
	}
    }

    fclose (ct->c_fp);
    ct->c_fp = NULL;
    return OK;
}


/*
 * reverse the order of the parts of a multipart
 */

static void
reverse_parts (CT ct)
{
    int i;
    struct multipart *m;
    struct part **base, **bmp, **next, *part;

    m = (struct multipart *) ct->c_ctparams;

    /* if only one part, just return */
    if (!m->mp_parts || !m->mp_parts->mp_next)
	return;

    /* count number of parts */
    i = 0;
    for (part = m->mp_parts; part; part = part->mp_next)
	i++;

    /* allocate array of pointers to the parts */
    if (!(base = (struct part **) calloc ((size_t) (i + 1), sizeof(*base))))
	adios (NULL, "out of memory");
    bmp = base;

    /* point at all the parts */
    for (part = m->mp_parts; part; part = part->mp_next)
	*bmp++ = part;
    *bmp = NULL;

    /* reverse the order of the parts */
    next = &m->mp_parts;
    for (bmp--; bmp >= base; bmp--) {
	part = *bmp;
	*next = part;
	next = &part->mp_next;
    }
    *next = NULL;

    /* free array of pointers */
    free ((char *) base);
}


/*
 * MESSAGE
 */

static int
InitMessage (CT ct)
{
    struct k2v *kv;
    CI ci = &ct->c_ctinfo;

    if ((ct->c_encoding != CE_7BIT) && (ct->c_encoding != CE_8BIT)) {
	admonish (NULL,
		  "\"%s/%s\" type in message %s should be encoded in 7bit or 8bit",
		  ci->ci_type, ci->ci_subtype, ct->c_file);
	return NOTOK;
    }

    /* check for missing subtype */
    if (!*ci->ci_subtype)
	ci->ci_subtype = add ("rfc822", ci->ci_subtype);

    /* match subtype */
    for (kv = SubMessage; kv->kv_key; kv++)
	if (!strcasecmp (ci->ci_subtype, kv->kv_key))
	    break;
    ct->c_subtype = kv->kv_value;

    switch (ct->c_subtype) {
	case MESSAGE_RFC822:
	    break;

	case MESSAGE_PARTIAL:
	    {
		char **ap, **ep;
		struct partial *p;

		if ((p = (struct partial *) calloc (1, sizeof(*p))) == NULL)
		    adios (NULL, "out of memory");
		ct->c_ctparams = (void *) p;

		/* scan for parameters "id", "number", and "total" */
		for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
		    if (!strcasecmp (*ap, "id")) {
			p->pm_partid = add (*ep, NULL);
			continue;
		    }
		    if (!strcasecmp (*ap, "number")) {
			if (sscanf (*ep, "%d", &p->pm_partno) != 1
			        || p->pm_partno < 1) {
invalid_param:
			    advise (NULL,
				    "invalid %s parameter for \"%s/%s\" type in message %s's %s field",
				    *ap, ci->ci_type, ci->ci_subtype,
				    ct->c_file, TYPE_FIELD);
			    return NOTOK;
			}
			continue;
		    }
		    if (!strcasecmp (*ap, "total")) {
			if (sscanf (*ep, "%d", &p->pm_maxno) != 1
			        || p->pm_maxno < 1)
			    goto invalid_param;
			continue;
		    }
		}

		if (!p->pm_partid
		        || !p->pm_partno
		        || (p->pm_maxno && p->pm_partno > p->pm_maxno)) {
		    advise (NULL,
			    "invalid parameters for \"%s/%s\" type in message %s's %s field",
			    ci->ci_type, ci->ci_subtype,
			    ct->c_file, TYPE_FIELD);
		    return NOTOK;
		}
	    }
	    break;

	case MESSAGE_EXTERNAL:
	    {
		int exresult;
		struct exbody *e;
		CT p;
		FILE *fp;

		if ((e = (struct exbody *) calloc (1, sizeof(*e))) == NULL)
		    adios (NULL, "out of memory");
		ct->c_ctparams = (void *) e;

		if (!ct->c_fp
		        && (ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
		    advise (ct->c_file, "unable to open for reading");
		    return NOTOK;
		}

		fseek (fp = ct->c_fp, ct->c_begin, SEEK_SET);

		if (!(p = get_content (fp, ct->c_file, 0))) {
		    fclose (ct->c_fp);
		    ct->c_fp = NULL;
		    return NOTOK;
		}

		e->eb_parent = ct;
		e->eb_content = p;
		p->c_ctexbody = e;
		if ((exresult = params_external (ct, 0)) != NOTOK
		        && p->c_ceopenfnx == openMail) {
		    int	cc, size;
		    char *bp;
		    
		    if ((size = ct->c_end - p->c_begin) <= 0) {
			if (!e->eb_subject)
			    content_error (NULL, ct,
					   "empty body for access-type=mail-server");
			goto no_body;
		    }
		    
		    if ((e->eb_body = bp = malloc ((unsigned) size)) == NULL)
			adios (NULL, "out of memory");
		    fseek (p->c_fp, p->c_begin, SEEK_SET);
		    while (size > 0)
			switch (cc = fread (bp, sizeof(*bp), size, p->c_fp)) {
			    case NOTOK:
			        adios ("failed", "fread");

			    case OK:
				adios (NULL, "unexpected EOF from fread");

			    default:
				bp += cc, size -= cc;
				break;
			}
		    *bp = 0;
		}
no_body:
		p->c_fp = NULL;
		p->c_end = p->c_begin;

		fclose (ct->c_fp);
		ct->c_fp = NULL;

		if (exresult == NOTOK)
		    return NOTOK;
		if (e->eb_flags == NOTOK)
		    return OK;

		switch (p->c_type) {
		    case CT_MULTIPART:
		        break;

		    case CT_MESSAGE:
			if (p->c_subtype != MESSAGE_RFC822)
			    break;
			/* else fall... */
		    default:
			e->eb_partno = ct->c_partno;
			if (p->c_ctinitfnx)
			    (*p->c_ctinitfnx) (p);
			break;
		}
	    }
	    break;

	default:
	    break;
    }

    return OK;
}


static int
params_external (CT ct, int composing)
{
    char **ap, **ep;
    struct exbody *e = (struct exbody *) ct->c_ctparams;
    CI ci = &ct->c_ctinfo;

    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
	if (!strcasecmp (*ap, "access-type")) {
	    struct str2init *s2i;
	    CT p = e->eb_content;

	    for (s2i = str2methods; s2i->si_key; s2i++)
		if (!strcasecmp (*ep, s2i->si_key))
		    break;

	    if (!s2i->si_key) {
		e->eb_access = *ep;
		e->eb_flags = NOTOK;
		p->c_encoding = CE_EXTERNAL;
		continue;
	    }
	    e->eb_access = s2i->si_key;
	    e->eb_flags = s2i->si_val;
	    p->c_encoding = CE_EXTERNAL;

	    /* Call the Init function for this external type */
	    if ((*s2i->si_init)(p) == NOTOK)
		return NOTOK;
	    continue;
	}
	if (!strcasecmp (*ap, "name")) {
	    e->eb_name = *ep;
	    continue;
	}
	if (!strcasecmp (*ap, "permission")) {
	    e->eb_permission = *ep;
	    continue;
	}
	if (!strcasecmp (*ap, "site")) {
	    e->eb_site = *ep;
	    continue;
	}
	if (!strcasecmp (*ap, "directory")) {
	    e->eb_dir = *ep;
	    continue;
	}
	if (!strcasecmp (*ap, "mode")) {
	    e->eb_mode = *ep;
	    continue;
	}
	if (!strcasecmp (*ap, "size")) {
	    sscanf (*ep, "%lu", &e->eb_size);
	    continue;
	}
	if (!strcasecmp (*ap, "server")) {
	    e->eb_server = *ep;
	    continue;
	}
	if (!strcasecmp (*ap, "subject")) {
	    e->eb_subject = *ep;
	    continue;
	}
	if (composing && !strcasecmp (*ap, "body")) {
	    e->eb_body = getcpy (*ep);
	    continue;
	}
    }

    if (!e->eb_access) {
	advise (NULL,
		"invalid parameters for \"%s/%s\" type in message %s's %s field",
		ci->ci_type, ci->ci_subtype, ct->c_file, TYPE_FIELD);
	return NOTOK;
    }

    return OK;
}


/*
 * APPLICATION
 */

static int
InitApplication (CT ct)
{
    struct k2v *kv;
    CI ci = &ct->c_ctinfo;

    /* match subtype */
    for (kv = SubApplication; kv->kv_key; kv++)
	if (!strcasecmp (ci->ci_subtype, kv->kv_key))
	    break;
    ct->c_subtype = kv->kv_value;

    return OK;
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


/*
 * TRANSFER ENCODINGS
 */

static int
init_encoding (CT ct, OpenCEFunc openfnx)
{
    CE ce;

    if ((ce = (CE) calloc (1, sizeof(*ce))) == NULL)
	adios (NULL, "out of memory");

    ct->c_cefile     = ce;
    ct->c_ceopenfnx  = openfnx;
    ct->c_ceclosefnx = close_encoding;
    ct->c_cesizefnx  = size_encoding;

    return OK;
}


static void
close_encoding (CT ct)
{
    CE ce;

    if (!(ce = ct->c_cefile))
	return;

    if (ce->ce_fp) {
	fclose (ce->ce_fp);
	ce->ce_fp = NULL;
    }
}


static unsigned long
size_encoding (CT ct)
{
    int	fd;
    unsigned long size;
    char *file;
    CE ce;
    struct stat st;

    if (!(ce = ct->c_cefile))
	return (ct->c_end - ct->c_begin);

    if (ce->ce_fp && fstat (fileno (ce->ce_fp), &st) != NOTOK)
	return (long) st.st_size;

    if (ce->ce_file) {
	if (stat (ce->ce_file, &st) != NOTOK)
	    return (long) st.st_size;
	else
	    return 0L;
    }

    if (ct->c_encoding == CE_EXTERNAL)
	return (ct->c_end - ct->c_begin);	

    file = NULL;
    if ((fd = (*ct->c_ceopenfnx) (ct, &file)) == NOTOK)
	return (ct->c_end - ct->c_begin);

    if (fstat (fd, &st) != NOTOK)
	size = (long) st.st_size;
    else
	size = 0L;

    (*ct->c_ceclosefnx) (ct);
    return size;
}


/*
 * BASE64
 */

static unsigned char b642nib[0x80] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff
};


static int
InitBase64 (CT ct)
{
    return init_encoding (ct, openBase64);
}


static int
openBase64 (CT ct, char **file)
{
    int	bitno, cc, digested;
    int fd, len, skip;
    unsigned long bits;
    unsigned char value, *b, *b1, *b2, *b3;
    char *cp, *ep, buffer[BUFSIZ];
    CE ce;
    MD5_CTX mdContext;

    b  = (unsigned char *) &bits;
    b1 = &b[endian > 0 ? 1 : 2];
    b2 = &b[endian > 0 ? 2 : 1];
    b3 = &b[endian > 0 ? 3 : 0];

    ce = ct->c_cefile;
    if (ce->ce_fp) {
	fseek (ce->ce_fp, 0L, SEEK_SET);
	goto ready_to_go;
    }

    if (ce->ce_file) {
	if ((ce->ce_fp = fopen (ce->ce_file, "r")) == NULL) {
	    content_error (ce->ce_file, ct, "unable to fopen for reading");
	    return NOTOK;
	}
	goto ready_to_go;
    }

    if (*file == NULL) {
	ce->ce_file = add (m_scratch ("", tmp), NULL);
	ce->ce_unlink = 1;
    } else {
	ce->ce_file = add (*file, NULL);
	ce->ce_unlink = 0;
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

    if ((len = ct->c_end - ct->c_begin) < 0)
	adios (NULL, "internal error(1)");

    if (!ct->c_fp && (ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
	content_error (ct->c_file, ct, "unable to open for reading");
	return NOTOK;
    }
    
    if ((digested = ct->c_digested))
	MD5Init (&mdContext);

    bitno = 18;
    bits = 0L;
    skip = 0;

    lseek (fd = fileno (ct->c_fp), (off_t) ct->c_begin, SEEK_SET);
    while (len > 0) {
	switch (cc = read (fd, buffer, sizeof(buffer) - 1)) {
	case NOTOK:
	    content_error (ct->c_file, ct, "error reading from");
	    goto clean_up;

	case OK:
	    content_error (NULL, ct, "premature eof");
	    goto clean_up;

	default:
	    if (cc > len)
		cc = len;
	    len -= cc;

	    for (ep = (cp = buffer) + cc; cp < ep; cp++) {
		switch (*cp) {
		default:
		    if (isspace (*cp))
			break;
		    if (skip || (*cp & 0x80)
			|| (value = b642nib[*cp & 0x7f]) > 0x3f) {
			if (debugsw) {
			    fprintf (stderr, "*cp=0x%x pos=%ld skip=%d\n",
				*cp,
				(long) (lseek (fd, (off_t) 0, SEEK_CUR) - (ep - cp)),
				skip);
			}
			content_error (NULL, ct,
				       "invalid BASE64 encoding -- continuing");
			continue;
		    }

		    bits |= value << bitno;
test_end:
		    if ((bitno -= 6) < 0) {
			putc ((char) *b1, ce->ce_fp);
			if (digested)
			    MD5Update (&mdContext, b1, 1);
			if (skip < 2) {
			    putc ((char) *b2, ce->ce_fp);
			    if (digested)
				MD5Update (&mdContext, b2, 1);
			    if (skip < 1) {
				putc ((char) *b3, ce->ce_fp);
				if (digested)
				    MD5Update (&mdContext, b3, 1);
			    }
			}

			if (ferror (ce->ce_fp)) {
			    content_error (ce->ce_file, ct,
					   "error writing to");
			    goto clean_up;
			}
			bitno = 18, bits = 0L, skip = 0;
		    }
		    break;

		case '=':
		    if (++skip > 3)
			goto self_delimiting;
		    goto test_end;
		}
	    }
	}
    }

    if (bitno != 18) {
	if (debugsw)
	    fprintf (stderr, "premature ending (bitno %d)\n", bitno);

	content_error (NULL, ct, "invalid BASE64 encoding");
	goto clean_up;
    }

self_delimiting:
    fseek (ct->c_fp, 0L, SEEK_SET);

    if (fflush (ce->ce_fp)) {
	content_error (ce->ce_file, ct, "error writing to");
	goto clean_up;
    }

    if (digested) {
	unsigned char digest[16];

	MD5Final (digest, &mdContext);
	if (memcmp((char *) digest, (char *) ct->c_digest,
		   sizeof(digest) / sizeof(digest[0])))
	    content_error (NULL, ct,
			   "content integrity suspect (digest mismatch) -- continuing");
	else
	    if (debugsw)
		fprintf (stderr, "content integrity confirmed\n");
    }

    fseek (ce->ce_fp, 0L, SEEK_SET);

ready_to_go:
    *file = ce->ce_file;
    return fileno (ce->ce_fp);

clean_up:
    free_encoding (ct, 0);
    return NOTOK;
}


/*
 * QUOTED PRINTABLE
 */

static char hex2nib[0x80] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


static int 
InitQuoted (CT ct)
{
    return init_encoding (ct, openQuoted);
}


static int
openQuoted (CT ct, char **file)
{
    int	cc, digested, len, quoted;
    char *cp, *ep;
    char buffer[BUFSIZ];
    unsigned char mask;
    CE ce;
    MD5_CTX mdContext;

    ce = ct->c_cefile;
    if (ce->ce_fp) {
	fseek (ce->ce_fp, 0L, SEEK_SET);
	goto ready_to_go;
    }

    if (ce->ce_file) {
	if ((ce->ce_fp = fopen (ce->ce_file, "r")) == NULL) {
	    content_error (ce->ce_file, ct, "unable to fopen for reading");
	    return NOTOK;
	}
	goto ready_to_go;
    }

    if (*file == NULL) {
	ce->ce_file = add (m_scratch ("", tmp), NULL);
	ce->ce_unlink = 1;
    } else {
	ce->ce_file = add (*file, NULL);
	ce->ce_unlink = 0;
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

    if ((len = ct->c_end - ct->c_begin) < 0)
	adios (NULL, "internal error(2)");

    if (!ct->c_fp && (ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
	content_error (ct->c_file, ct, "unable to open for reading");
	return NOTOK;
    }

    if ((digested = ct->c_digested))
	MD5Init (&mdContext);

    quoted = 0;
#ifdef lint
    mask = 0;
#endif

    fseek (ct->c_fp, ct->c_begin, SEEK_SET);
    while (len > 0) {
	char *dp;

	if (fgets (buffer, sizeof(buffer) - 1, ct->c_fp) == NULL) {
	    content_error (NULL, ct, "premature eof");
	    goto clean_up;
	}

	if ((cc = strlen (buffer)) > len)
	    cc = len;
	len -= cc;

	for (ep = (cp = buffer) + cc - 1; cp <= ep; ep--)
	    if (!isspace (*ep))
		break;
	*++ep = '\n', ep++;

	for (; cp < ep; cp++) {
	    if (quoted) {
		if (quoted > 1) {
		    if (!isxdigit (*cp)) {
invalid_hex:
			dp = "expecting hexidecimal-digit";
			goto invalid_encoding;
		    }
		    mask <<= 4;
		    mask |= hex2nib[*cp & 0x7f];
		    putc (mask, ce->ce_fp);
		    if (digested)
			MD5Update (&mdContext, &mask, 1);
		} else {
		    switch (*cp) {
		    case ':':
			putc (*cp, ce->ce_fp);
			if (digested)
			    MD5Update (&mdContext, (unsigned char *) ":", 1);
			break;

		    default:
			if (!isxdigit (*cp))
			    goto invalid_hex;
			mask = hex2nib[*cp & 0x7f];
			quoted = 2;
			continue;
		    }
		}

		if (ferror (ce->ce_fp)) {
		    content_error (ce->ce_file, ct, "error writing to");
		    goto clean_up;
		}
		quoted = 0;
		continue;
	    }

	    switch (*cp) {
	    default:
		if (*cp < '!' || *cp > '~') {
		    int	i;
		    dp = "expecting character in range [!..~]";

invalid_encoding:
		    i = strlen (invo_name) + 2;
		    content_error (NULL, ct,
				   "invalid QUOTED-PRINTABLE encoding -- %s,\n%*.*sbut got char 0x%x",
				   dp, i, i, "", *cp);
		    goto clean_up;
		}
		/* and fall...*/
	    case ' ':
	    case '\t':
	    case '\n':
		putc (*cp, ce->ce_fp);
		if (digested) {
		    if (*cp == '\n')
			MD5Update (&mdContext, (unsigned char *) "\r\n",2);
		    else
			MD5Update (&mdContext, (unsigned char *) cp, 1);
		}
		if (ferror (ce->ce_fp)) {
		    content_error (ce->ce_file, ct, "error writing to");
		    goto clean_up;
		}
		break;

	    case '=':
		if (*++cp != '\n') {
		    quoted = 1;
		    cp--;
		}
		break;
	    }
	}
    }
    if (quoted) {
	content_error (NULL, ct,
		       "invalid QUOTED-PRINTABLE encoding -- end-of-content while still quoting");
	goto clean_up;
    }

    fseek (ct->c_fp, 0L, SEEK_SET);

    if (fflush (ce->ce_fp)) {
	content_error (ce->ce_file, ct, "error writing to");
	goto clean_up;
    }

    if (digested) {
	unsigned char digest[16];

	MD5Final (digest, &mdContext);
	if (memcmp((char *) digest, (char *) ct->c_digest,
		   sizeof(digest) / sizeof(digest[0])))
	    content_error (NULL, ct,
			   "content integrity suspect (digest mismatch) -- continuing");
	else
	    if (debugsw)
		fprintf (stderr, "content integrity confirmed\n");
    }

    fseek (ce->ce_fp, 0L, SEEK_SET);

ready_to_go:
    *file = ce->ce_file;
    return fileno (ce->ce_fp);

clean_up:
    free_encoding (ct, 0);
    return NOTOK;
}


/*
 * 7BIT
 */

static int
Init7Bit (CT ct)
{
    if (init_encoding (ct, open7Bit) == NOTOK)
	return NOTOK;

    ct->c_cesizefnx = NULL;	/* no need to decode for real size */
    return OK;
}


static int
open7Bit (CT ct, char **file)
{
    int	cc, fd, len;
    char buffer[BUFSIZ];
    CE ce;

    ce = ct->c_cefile;
    if (ce->ce_fp) {
	fseek (ce->ce_fp, 0L, SEEK_SET);
	goto ready_to_go;
    }

    if (ce->ce_file) {
	if ((ce->ce_fp = fopen (ce->ce_file, "r")) == NULL) {
	    content_error (ce->ce_file, ct, "unable to fopen for reading");
	    return NOTOK;
	}
	goto ready_to_go;
    }

    if (*file == NULL) {
	ce->ce_file = add (m_scratch ("", tmp), NULL);
	ce->ce_unlink = 1;
    } else {
	ce->ce_file = add (*file, NULL);
	ce->ce_unlink = 0;
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

    if (ct->c_type == CT_MULTIPART) {
	char **ap, **ep;
	CI ci = &ct->c_ctinfo;

	len = 0;
	fprintf (ce->ce_fp, "%s: %s/%s", TYPE_FIELD, ci->ci_type, ci->ci_subtype);
	len += strlen (TYPE_FIELD) + 2 + strlen (ci->ci_type)
	    + 1 + strlen (ci->ci_subtype);
	for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
	    putc (';', ce->ce_fp);
	    len++;

	    snprintf (buffer, sizeof(buffer), "%s=\"%s\"", *ap, *ep);

	    if (len + 1 + (cc = strlen (buffer)) >= CPERLIN) {
		fputs ("\n\t", ce->ce_fp);
		len = 8;
	    } else {
		putc (' ', ce->ce_fp);
		len++;
	    }
	    fprintf (ce->ce_fp, "%s", buffer);
	    len += cc;
	}

	if (ci->ci_comment) {
	    if (len + 1 + (cc = 2 + strlen (ci->ci_comment)) >= CPERLIN) {
		fputs ("\n\t", ce->ce_fp);
		len = 8;
	    }
	    else {
		putc (' ', ce->ce_fp);
		len++;
	    }
	    fprintf (ce->ce_fp, "(%s)", ci->ci_comment);
	    len += cc;
	}
	fprintf (ce->ce_fp, "\n");
	if (ct->c_id)
	    fprintf (ce->ce_fp, "%s:%s", ID_FIELD, ct->c_id);
	if (ct->c_descr)
	    fprintf (ce->ce_fp, "%s:%s", DESCR_FIELD, ct->c_descr);
	fprintf (ce->ce_fp, "\n");
    }

    if ((len = ct->c_end - ct->c_begin) < 0)
	adios (NULL, "internal error(3)");

    if (!ct->c_fp && (ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
	content_error (ct->c_file, ct, "unable to open for reading");
	return NOTOK;
    }

    lseek (fd = fileno (ct->c_fp), (off_t) ct->c_begin, SEEK_SET);
    while (len > 0)
	switch (cc = read (fd, buffer, sizeof(buffer) - 1)) {
	case NOTOK:
	    content_error (ct->c_file, ct, "error reading from");
	    goto clean_up;

	case OK:
	    content_error (NULL, ct, "premature eof");
	    goto clean_up;

	default:
	    if (cc > len)
		cc = len;
	    len -= cc;

	    fwrite (buffer, sizeof(*buffer), cc, ce->ce_fp);
	    if (ferror (ce->ce_fp)) {
		content_error (ce->ce_file, ct, "error writing to");
		goto clean_up;
	    }
	}

    fseek (ct->c_fp, 0L, SEEK_SET);

    if (fflush (ce->ce_fp)) {
	content_error (ce->ce_file, ct, "error writing to");
	goto clean_up;
    }

    fseek (ce->ce_fp, 0L, SEEK_SET);

ready_to_go:
    *file = ce->ce_file;
    return fileno (ce->ce_fp);

clean_up:
    free_encoding (ct, 0);
    return NOTOK;
}


/*
 * External
 */

static int
openExternal (CT ct, CT cb, CE ce, char **file, int *fd)
{
    char cachefile[BUFSIZ];

    if (ce->ce_fp) {
	fseek (ce->ce_fp, 0L, SEEK_SET);
	goto ready_already;
    }

    if (ce->ce_file) {
	if ((ce->ce_fp = fopen (ce->ce_file, "r")) == NULL) {
	    content_error (ce->ce_file, ct, "unable to fopen for reading");
	    return NOTOK;
	}
	goto ready_already;
    }

    if (find_cache (ct, rcachesw, (int *) 0, cb->c_id,
		cachefile, sizeof(cachefile)) != NOTOK) {
	if ((ce->ce_fp = fopen (cachefile, "r"))) {
	    ce->ce_file = getcpy (cachefile);
	    ce->ce_unlink = 0;
	    goto ready_already;
	} else {
	    admonish (cachefile, "unable to fopen for reading");
	}
    }

    return OK;

ready_already:
    *file = ce->ce_file;
    *fd = fileno (ce->ce_fp);
    return DONE;
}

/*
 * File
 */

static int
InitFile (CT ct)
{
    return init_encoding (ct, openFile);
}


static int
openFile (CT ct, char **file)
{
    int	fd, cachetype;
    char cachefile[BUFSIZ];
    struct exbody *e = ct->c_ctexbody;
    CE ce = ct->c_cefile;

    switch (openExternal (e->eb_parent, e->eb_content, ce, file, &fd)) {
	case NOTOK:
	    return NOTOK;

	case OK:
	    break;

	case DONE:
	    return fd;
    }

    if (!e->eb_name) {
	content_error (NULL, ct, "missing name parameter");
	return NOTOK;
    }

    ce->ce_file = getcpy (e->eb_name);
    ce->ce_unlink = 0;

    if ((ce->ce_fp = fopen (ce->ce_file, "r")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading");
	return NOTOK;
    }

    if ((!e->eb_permission || strcasecmp (e->eb_permission, "read-write"))
	    && find_cache (NULL, wcachesw, &cachetype, e->eb_content->c_id,
		cachefile, sizeof(cachefile)) != NOTOK) {
	int mask;
	FILE *fp;

	mask = umask (cachetype ? ~m_gmprot () : 0222);
	if ((fp = fopen (cachefile, "w"))) {
	    int	cc;
	    char buffer[BUFSIZ];
	    FILE *gp = ce->ce_fp;

	    fseek (gp, 0L, SEEK_SET);

	    while ((cc = fread (buffer, sizeof(*buffer), sizeof(buffer), gp))
		       > 0)
		fwrite (buffer, sizeof(*buffer), cc, fp);
	    fflush (fp);

	    if (ferror (gp)) {
		admonish (ce->ce_file, "error reading");
		unlink (cachefile);
	    }
	    else
		if (ferror (fp)) {
		    admonish (cachefile, "error writing");
		    unlink (cachefile);
		}
	    fclose (fp);
	}
	umask (mask);
    }

    fseek (ce->ce_fp, 0L, SEEK_SET);
    *file = ce->ce_file;
    return fileno (ce->ce_fp);
}

/*
 * FTP
 */

static int
InitFTP (CT ct)
{
    return init_encoding (ct, openFTP);
}


static int
openFTP (CT ct, char **file)
{
    int	cachetype, caching, fd;
    int len, buflen;
    char *bp, *ftp, *user, *pass;
    char buffer[BUFSIZ], cachefile[BUFSIZ];
    struct exbody *e;
    CE ce;
    static char *username = NULL;
    static char *password = NULL;

    e  = ct->c_ctexbody;
    ce = ct->c_cefile;

    if ((ftp = context_find (nmhaccessftp)) && !*ftp)
	ftp = NULL;

#ifndef BUILTIN_FTP
    if (!ftp)
	return NOTOK;
#endif

    switch (openExternal (e->eb_parent, e->eb_content, ce, file, &fd)) {
	case NOTOK:
	    return NOTOK;

	case OK:
	    break;

	case DONE:
	    return fd;
    }

    if (!e->eb_name || !e->eb_site) {
	content_error (NULL, ct, "missing %s parameter",
		       e->eb_name ? "site": "name");
	return NOTOK;
    }

    if (xpid) {
	if (xpid < 0)
	    xpid = -xpid;
	pidcheck (pidwait (xpid, NOTOK));
	xpid = 0;
    }

    /* Get the buffer ready to go */
    bp = buffer;
    buflen = sizeof(buffer);

    /*
     * Construct the query message for user
     */
    snprintf (bp, buflen, "Retrieve %s", e->eb_name);
    len = strlen (bp);
    bp += len;
    buflen -= len;

    if (e->eb_partno) {
	snprintf (bp, buflen, " (content %s)", e->eb_partno);
	len = strlen (bp);
	bp += len;
	buflen -= len;
    }

    snprintf (bp, buflen, "\n    using %sFTP from site %s",
		    e->eb_flags ? "anonymous " : "", e->eb_site);
    len = strlen (bp);
    bp += len;
    buflen -= len;

    if (e->eb_size > 0) {
	snprintf (bp, buflen, " (%lu octets)", e->eb_size);
	len = strlen (bp);
	bp += len;
	buflen -= len;
    }
    snprintf (bp, buflen, "? ");

    /*
     * Now, check the answer
     */
    if (!getanswer (buffer))
	return NOTOK;

    if (e->eb_flags) {
	user = "anonymous";
	snprintf (buffer, sizeof(buffer), "%s@%s", getusername (), LocalName ());
	pass = buffer;
    } else {
	ruserpass (e->eb_site, &username, &password);
	user = username;
	pass = password;
    }

    ce->ce_unlink = (*file == NULL);
    caching = 0;
    cachefile[0] = '\0';
    if ((!e->eb_permission || strcasecmp (e->eb_permission, "read-write"))
	    && find_cache (NULL, wcachesw, &cachetype, e->eb_content->c_id,
		cachefile, sizeof(cachefile)) != NOTOK) {
	if (*file == NULL) {
	    ce->ce_unlink = 0;
	    caching = 1;
	}
    }

    if (*file)
	ce->ce_file = add (*file, NULL);
    else if (caching)
	ce->ce_file = add (cachefile, NULL);
    else
	ce->ce_file = add (m_scratch ("", tmp), NULL);

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

#ifdef BUILTIN_FTP
    if (ftp)
#endif
    {
	int child_id, i, vecp;
	char *vec[9];

	vecp = 0;
	vec[vecp++] = r1bindex (ftp, '/');
	vec[vecp++] = e->eb_site;
	vec[vecp++] = user;
	vec[vecp++] = pass;
	vec[vecp++] = e->eb_dir;
	vec[vecp++] = e->eb_name;
	vec[vecp++] = ce->ce_file,
	vec[vecp++] = e->eb_mode && !strcasecmp (e->eb_mode, "ascii")
	    		? "ascii" : "binary";
	vec[vecp] = NULL;

	fflush (stdout);

	for (i = 0; (child_id = vfork ()) == NOTOK && i < 5; i++)
	    sleep (5);
	switch (child_id) {
	    case NOTOK:
	        adios ("fork", "unable to");
		/* NOTREACHED */

	    case OK:
		close (fileno (ce->ce_fp));
		execvp (ftp, vec);
		fprintf (stderr, "unable to exec ");
		perror (ftp);
		_exit (-1);
		/* NOTREACHED */

	    default:
		if (pidXwait (child_id, NULL)) {
#ifdef BUILTIN_FTP
losing_ftp:
#endif
		    username = password = NULL;
		    ce->ce_unlink = 1;
		    return NOTOK;
		}
		break;
	}
    }
#ifdef BUILTIN_FTP
    else
	if (ftp_get (e->eb_site, user, pass, e->eb_dir, e->eb_name,
		     ce->ce_file,
		     e->eb_mode && !strcasecmp (e->eb_mode, "ascii"), 0)
	        == NOTOK)
	    goto losing_ftp;
#endif

    if (cachefile[0]) {
	if (caching)
	    chmod (cachefile, cachetype ? m_gmprot () : 0444);
	else {
	    int	mask;
	    FILE *fp;

	    mask = umask (cachetype ? ~m_gmprot () : 0222);
	    if ((fp = fopen (cachefile, "w"))) {
		int cc;
		FILE *gp = ce->ce_fp;

		fseek (gp, 0L, SEEK_SET);

		while ((cc= fread (buffer, sizeof(*buffer), sizeof(buffer), gp))
		           > 0)
		    fwrite (buffer, sizeof(*buffer), cc, fp);
		fflush (fp);

		if (ferror (gp)) {
		    admonish (ce->ce_file, "error reading");
		    unlink (cachefile);
		}
		else
		    if (ferror (fp)) {
			admonish (cachefile, "error writing");
			unlink (cachefile);
		    }
		fclose (fp);
	    }
	    umask (mask);
	}
    }

    fseek (ce->ce_fp, 0L, SEEK_SET);
    *file = ce->ce_file;
    return fileno (ce->ce_fp);
}


/*
 * Mail
 */

static int
InitMail (CT ct)
{
    return init_encoding (ct, openMail);
}


static int
openMail (CT ct, char **file)
{
    int	child_id, fd, i, vecp;
    int len, buflen;
    char *bp, buffer[BUFSIZ], *vec[7];
    struct exbody *e = ct->c_ctexbody;
    CE ce = ct->c_cefile;

    switch (openExternal (e->eb_parent, e->eb_content, ce, file, &fd)) {
	case NOTOK:
	    return NOTOK;

	case OK:
	    break;

	case DONE:
	    return fd;
    }

    if (!e->eb_server) {
	content_error (NULL, ct, "missing server parameter");
	return NOTOK;
    }

    if (xpid) {
	if (xpid < 0)
	    xpid = -xpid;
	pidcheck (pidwait (xpid, NOTOK));
	xpid = 0;
    }

    /* Get buffer ready to go */
    bp = buffer;
    buflen = sizeof(buffer);

    /* Now construct query message */
    snprintf (bp, buflen, "Retrieve content");
    len = strlen (bp);
    bp += len;
    buflen -= len;

    if (e->eb_partno) {
	snprintf (bp, buflen, " %s", e->eb_partno);
	len = strlen (bp);
	bp += len;
	buflen -= len;
    }

    snprintf (bp, buflen, " by asking %s\n\n%s\n? ",
		    e->eb_server,
		    e->eb_subject ? e->eb_subject : e->eb_body);

    /* Now, check answer */
    if (!getanswer (buffer))
	return NOTOK;

    vecp = 0;
    vec[vecp++] = r1bindex (mailproc, '/');
    vec[vecp++] = e->eb_server;
    vec[vecp++] = "-subject";
    vec[vecp++] = e->eb_subject ? e->eb_subject : "mail-server request";
    vec[vecp++] = "-body";
    vec[vecp++] = e->eb_body;
    vec[vecp] = NULL;

    for (i = 0; (child_id = vfork ()) == NOTOK && i < 5; i++)
	sleep (5);
    switch (child_id) {
	case NOTOK:
	    advise ("fork", "unable to");
	    return NOTOK;

	case OK:
	    execvp (mailproc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (mailproc);
	    _exit (-1);
	    /* NOTREACHED */

	default:
	    if (pidXwait (child_id, NULL) == OK)
		advise (NULL, "request sent");
	    break;
    }

    if (*file == NULL) {
	ce->ce_file = add (m_scratch ("", tmp), NULL);
	ce->ce_unlink = 1;
    } else {
	ce->ce_file = add (*file, NULL);
	ce->ce_unlink = 0;
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

    fseek (ce->ce_fp, 0L, SEEK_SET);
    *file = ce->ce_file;
    return fileno (ce->ce_fp);
}


static char *
fgetstr (char *s, int n, FILE *stream)
{
    char *cp, *ep;

    for (ep = (cp = s) + n; cp < ep; ) {
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
    char *cp, **ap;
    char buffer[BUFSIZ];
    struct multipart *m;
    struct part **pp;
    struct stat st;
    struct str2init *s2i;
    CI ci;
    CT ct;
    CE ce;

    if (buf[0] == '\n' || strcmp (buf, "#\n") == 0) {
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
    if (buf[0] != '#' || buf[1] == '#' || buf[1] == '<') {
	int headers;
	int inlineD;
	long pos;
	char content[BUFSIZ];
	FILE *out;

	/* use a temp file to collect the plain text lines */
	ce->ce_file = add (m_tmpfil (invo_name), NULL);
	ce->ce_unlink = 1;

	if ((out = fopen (ce->ce_file, "w")) == NULL)
	    adios (ce->ce_file, "unable to open for writing");

	if (buf[0] == '#' && buf[1] == '<') {
	    strncpy (content, buf + 2, sizeof(content));
	    inlineD = 1;
	    goto rock_and_roll;
	} else {
	    inlineD = 0;
	}

	/* the directive is implicit */
	strncpy (content, "text/plain", sizeof(content));
	headers = 0;
	strncpy (buffer, buf[0] != '#' ? buf : buf + 1, sizeof(buffer));
	for (;;) {
	    int	i;

	    if (headers >= 0 && uprf (buffer, DESCR_FIELD)
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

	    if (headers != 1 || buffer[0] != '\n')
		fputs (buffer, out);

rock_and_roll:
	    headers = -1;
	    pos = ftell (in);
	    if ((cp = fgetstr (buffer, sizeof(buffer) - 1, in)) == NULL)
		break;
	    if (buffer[0] == '#') {
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
		    folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
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
    char msgid[BUFSIZ];
    static int partno;
    static time_t clock = 0;
    static char *msgfmt;

    if (clock == 0) {
	time (&clock);
	snprintf (msgid, sizeof(msgid), "<%d.%ld.%%d@%s>\n",
		(int) getpid(), (long) clock, LocalName());
	partno = 0;
	msgfmt = getcpy(msgid);
    }
    snprintf (msgid, sizeof(msgid), msgfmt, top ? 0 : ++partno);
    ct->c_id = getcpy (msgid);
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

	    if (!(cp = ci->ci_magic))
		adios (NULL, "internal error(5)");

	    ce->ce_file = add (m_tmpfil (invo_name), NULL);
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
    int check8bit, contains8bit = 0;	  /* check if contains 8bit data                */
    int checklinelen, linelen = 0;	  /* check for long lines                       */
    int checkboundary, boundaryclash = 0; /* check if clashes with multipart boundary   */
    int checklinespace, linespace = 0;	  /* check if any line ends with space          */
    int checkebcdic, ebcdicunsafe = 0;	  /* check if contains ebcdic unsafe characters */
    char *cp, buffer[BUFSIZ];
    struct text *t;
    FILE *in;
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
		t->tx_charset = CHARSET_UNKNOWN;
		*ap = concat ("charset=", write_charset_8bit(), NULL);
	    } else {
		t->tx_charset = CHARSET_USASCII;
		*ap = add ("charset=us-ascii", NULL);
	    }

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
     * description, and Content-ID if the content is of type
     * "message" and the rfc934 compatibility flag is set
     * (which means we are inside multipart/digest and the
     * switch -rfc934mode was given).
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
	if (mailbody && !strcasecmp (*ap, "body"))
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
     * output the Content-ID
     */
    if (ct->c_id) {
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
    FILE *in;
    MD5_CTX mdContext;
    CE ce = ct->c_cefile;

    /* open content */
    if ((in = fopen (ce->ce_file, "r")) == NULL)
	adios (ce->ce_file, "unable to open for reading");

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


static int
readDigest (CT ct, char *cp)
{
    int	bitno, skip;
    unsigned long bits;
    char *bp = cp;
    unsigned char *dp, value, *ep;
    unsigned char *b, *b1, *b2, *b3;

    b  = (unsigned char *) &bits,
    b1 = &b[endian > 0 ? 1 : 2],
    b2 = &b[endian > 0 ? 2 : 1],
    b3 = &b[endian > 0 ? 3 : 0];
    bitno = 18;
    bits = 0L;
    skip = 0;

    for (ep = (dp = ct->c_digest)
	         + sizeof(ct->c_digest) / sizeof(ct->c_digest[0]); *cp; cp++)
	switch (*cp) {
	    default:
	        if (skip
		        || (*cp & 0x80)
		        || (value = b642nib[*cp & 0x7f]) > 0x3f) {
		    if (debugsw)
			fprintf (stderr, "invalid BASE64 encoding\n");
		    return NOTOK;
		}

		bits |= value << bitno;
test_end:
		if ((bitno -= 6) < 0) {
		    if (dp + (3 - skip) > ep)
			goto invalid_digest;
		    *dp++ = *b1;
		    if (skip < 2) {
			*dp++ = *b2;
			if (skip < 1)
			    *dp++ = *b3;
		    }
		    bitno = 18;
		    bits = 0L;
		    skip = 0;
		}
		break;

	    case '=':
		if (++skip > 3)
		    goto self_delimiting;
		goto test_end;
	}
    if (bitno != 18) {
	if (debugsw)
	    fprintf (stderr, "premature ending (bitno %d)\n", bitno);

	return NOTOK;
    }
self_delimiting:
    if (dp != ep) {
invalid_digest:
	if (debugsw) {
	    while (*cp)
		cp++;
	    fprintf (stderr, "invalid MD5 digest (got %d octets)\n",
		     cp - bp);
	}

	return NOTOK;
    }

    if (debugsw) {
	fprintf (stderr, "MD5 digest=");
	for (dp = ct->c_digest; dp < ep; dp++)
	    fprintf (stderr, "%02x", *dp & 0xff);
	fprintf (stderr, "\n");
    }

    return OK;
}
