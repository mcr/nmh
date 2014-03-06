
/*
 * mhparse.c -- routines to parse the contents of MIME messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/utils.h>
#ifdef HAVE_ICONV
# include <iconv.h>
#endif /* HAVE_ICONV */


extern int debugsw;

extern pid_t xpid;	/* mhshowsbr.c  */

/* cache policies */
extern int rcachesw;	/* mhcachesbr.c */
extern int wcachesw;	/* mhcachesbr.c */

int checksw = 0;	/* check Content-MD5 field */

/*
 * These are for mhfixmsg to:
 * 1) Instruct parser not to detect invalid Content-Transfer-Encoding
 *    in a multipart.
 * 2) Suppress the warning about bogus multipart content, and report it.
 */
int skip_mp_cte_check;
int suppress_bogus_mp_content_warning;
int bogus_mp_content;

/*
 * Structures for TEXT messages
 */
struct k2v SubText[] = {
    { "plain",    TEXT_PLAIN },
    { "richtext", TEXT_RICHTEXT },  /* defined in RFC-1341    */
    { "enriched", TEXT_ENRICHED },  /* defined in RFC-1896    */
    { NULL,       TEXT_UNKNOWN }    /* this one must be last! */
};

/* Charset[] removed -- yozo.  Mon Oct  8 01:03:41 JST 2012 */

/*
 * Structures for MULTIPART messages
 */
struct k2v SubMultiPart[] = {
    { "mixed",       MULTI_MIXED },
    { "alternative", MULTI_ALTERNATE },
    { "digest",      MULTI_DIGEST },
    { "parallel",    MULTI_PARALLEL },
    { NULL,          MULTI_UNKNOWN }    /* this one must be last! */
};

/*
 * Structures for MESSAGE messages
 */
struct k2v SubMessage[] = {
    { "rfc822",        MESSAGE_RFC822 },
    { "partial",       MESSAGE_PARTIAL },
    { "external-body", MESSAGE_EXTERNAL },
    { NULL,            MESSAGE_UNKNOWN }	/* this one must be last! */
};

/*
 * Structure for APPLICATION messages
 */
struct k2v SubApplication[] = {
    { "octet-stream", APPLICATION_OCTETS },
    { "postscript",   APPLICATION_POSTSCRIPT },
    { NULL,           APPLICATION_UNKNOWN }	/* this one must be last! */
};

/*
 * Mapping of names of CTE types in mhbuild directives
 */
static struct k2v EncodingType[] = {
    { "8bit",			CE_8BIT },
    { "qp",			CE_QUOTED },
    { "q-p",			CE_QUOTED },
    { "quoted-printable",	CE_QUOTED },
    { "b64",			CE_BASE64 },
    { "base64",			CE_BASE64 },
    { NULL,			0 },
};


/* mhcachesbr.c */
int find_cache (CT, int, int *, char *, char *, int);

/* mhmisc.c */
int part_ok (CT, int);
int type_ok (CT, int);
void content_error (char *, CT, char *, ...);

/* mhfree.c */
void free_encoding (CT, int);

/*
 * static prototypes
 */
static CT get_content (FILE *, char *, int);
static int get_comment (const char *, const char *, char **, char **);

static int InitGeneric (CT);
static int InitText (CT);
static int InitMultiPart (CT);
void reverse_parts (CT);
static int InitMessage (CT);
static int InitApplication (CT);
static int init_encoding (CT, OpenCEFunc);
static unsigned long size_encoding (CT);
static int InitBase64 (CT);
static int openBase64 (CT, char **);
static int InitQuoted (CT);
static int openQuoted (CT, char **);
static int Init7Bit (CT);
static int openExternal (CT, CT, CE, char **, int *);
static int InitFile (CT);
static int openFile (CT, char **);
static int InitFTP (CT);
static int openFTP (CT, char **);
static int InitMail (CT);
static int openMail (CT, char **);
static int readDigest (CT, char *);
static int get_leftover_mp_content (CT, int);
static int InitURL (CT);
static int openURL (CT, char **);
static int parse_header_attrs (const char *, const char *, char **, PM *,
			       PM *, char **);
static size_t param_len(PM, int, size_t, int *, int *, size_t *);
static size_t encode_param(PM, char *, size_t, size_t, size_t, int);
static size_t normal_param(PM, char *, size_t, size_t, size_t);
static int get_dispo (char *, CT, int);

struct str2init str2cts[] = {
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

struct str2init str2ces[] = {
    { "base64",		  CE_BASE64,	InitBase64 },
    { "quoted-printable", CE_QUOTED,	InitQuoted },
    { "8bit",		  CE_8BIT,	Init7Bit },
    { "7bit",		  CE_7BIT,	Init7Bit },
    { "binary",		  CE_BINARY,	Init7Bit },
    { NULL,		  CE_EXTENSION,	NULL },	 /* these two must be last! */
    { NULL,		  CE_UNKNOWN,	NULL },
};

/*
 * NOTE WELL: si_key MUST NOT have value of NOTOK
 *
 * si_key is 1 if access method is anonymous.
 */
struct str2init str2methods[] = {
    { "afs",         1,	InitFile },
    { "anon-ftp",    1,	InitFTP },
    { "ftp",         0,	InitFTP },
    { "local-file",  0,	InitFile },
    { "mail-server", 0,	InitMail },
    { "url",         0, InitURL },
    { NULL,          0, NULL }
};


int
pidcheck (int status)
{
    if ((status & 0xff00) == 0xff00 || (status & 0x007f) != SIGQUIT)
	return status;

    fflush (stdout);
    fflush (stderr);
    done (1);
    return 1;
}


/*
 * Main entry point for parsing a MIME message or file.
 * It returns the Content structure for the top level
 * entity in the file.
 */

CT
parse_mime (char *file)
{
    int is_stdin;
    char buffer[BUFSIZ];
    FILE *fp;
    CT ct;

    /*
     * Check if file is actually standard input
     */
    if ((is_stdin = !(strcmp (file, "-")))) {
        char *tfile = m_mktemp2(NULL, invo_name, NULL, &fp);
        if (tfile == NULL) {
            advise("mhparse", "unable to create temporary file in %s",
		   get_temp_dir());
            return NULL;
        }
	file = add (tfile, NULL);

	while (fgets (buffer, sizeof(buffer), stdin))
	    fputs (buffer, fp);
	fflush (fp);

	if (ferror (stdin)) {
	    (void) m_unlink (file);
	    advise ("stdin", "error reading");
	    return NULL;
	}
	if (ferror (fp)) {
	    (void) m_unlink (file);
	    advise (file, "error writing");
	    return NULL;
	}
	fseek (fp, 0L, SEEK_SET);
    } else if ((fp = fopen (file, "r")) == NULL) {
	advise (file, "unable to read");
	return NULL;
    }

    if (!(ct = get_content (fp, file, 1))) {
	if (is_stdin)
	    (void) m_unlink (file);
	advise (NULL, "unable to decode %s", file);
	return NULL;
    }

    if (is_stdin)
	ct->c_unlink = 1;	/* temp file to remove */

    ct->c_fp = NULL;

    if (ct->c_end == 0L) {
	fseek (fp, 0L, SEEK_END);
	ct->c_end = ftell (fp);
    }

    if (ct->c_ctinitfnx && (*ct->c_ctinitfnx) (ct) == NOTOK) {
	fclose (fp);
	free_content (ct);
	return NULL;
    }

    fclose (fp);
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
 * NB: on failure we will fclose(in)!
 */

static CT
get_content (FILE *in, char *file, int toplevel)
{
    int compnum, state;
    char buf[BUFSIZ], name[NAMESZ];
    char *np, *vp;
    CT ct;
    HF hp;
    m_getfld_state_t gstate = 0;

    /* allocate the content structure */
    if (!(ct = (CT) calloc (1, sizeof(*ct))))
	adios (NULL, "out of memory");

    ct->c_fp = in;
    ct->c_file = add (file, NULL);
    ct->c_begin = ftell (ct->c_fp) + 1;

    /*
     * Parse the header fields for this
     * content into a linked list.
     */
    m_getfld_track_filepos (&gstate, in);
    for (compnum = 1;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld (&gstate, name, buf, &bufsz, in)) {
	case FLD:
	case FLDPLUS:
	    compnum++;

	    /* get copies of the buffers */
	    np = add (name, NULL);
	    vp = add (buf, NULL);

	    /* if necessary, get rest of field */
	    while (state == FLDPLUS) {
		bufsz = sizeof buf;
		state = m_getfld (&gstate, name, buf, &bufsz, in);
		vp = add (buf, vp);	/* add to previous value */
	    }

	    /* Now add the header data to the list */
	    add_header (ct, np, vp);

	    /* continue, to see if this isn't the last header field */
	    ct->c_begin = ftell (in) + 1;
	    continue;

	case BODY:
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

	/* break out of the loop */
	break;
    }
    m_getfld_state_destroy (&gstate);

    /*
     * Read the content headers.  We will parse the
     * MIME related header fields into their various
     * structures and set internal flags related to
     * content type/subtype, etc.
     */

    hp = ct->c_first_hf;	/* start at first header field */
    while (hp) {
	/* Get MIME-Version field */
	if (!strcasecmp (hp->name, VRSN_FIELD)) {
	    int ucmp;
	    char c, *cp, *dp;

	    if (ct->c_vrsn) {
		advise (NULL, "message %s has multiple %s: fields",
			ct->c_file, VRSN_FIELD);
		goto next_header;
	    }
	    ct->c_vrsn = add (hp->value, NULL);

	    /* Now, cleanup this field */
	    cp = ct->c_vrsn;

	    while (isspace ((unsigned char) *cp))
		cp++;
	    for (dp = strchr(cp, '\n'); dp; dp = strchr(dp, '\n'))
		*dp++ = ' ';
	    for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
		if (!isspace ((unsigned char) *dp))
		    break;
	    *++dp = '\0';
	    if (debugsw)
		fprintf (stderr, "%s: %s\n", VRSN_FIELD, cp);

	    if (*cp == '('  &&
                get_comment (ct->c_file, VRSN_FIELD, &cp, NULL) == NOTOK)
		goto out;

	    for (dp = cp; istoken (*dp); dp++)
		continue;
	    c = *dp;
	    *dp = '\0';
	    ucmp = !strcasecmp (cp, VRSN_VALUE);
	    *dp = c;
	    if (!ucmp) {
		admonish (NULL, "message %s has unknown value for %s: field (%s)",
		ct->c_file, VRSN_FIELD, cp);
	    }
	}
	else if (!strcasecmp (hp->name, TYPE_FIELD)) {
	/* Get Content-Type field */
	    struct str2init *s2i;
	    CI ci = &ct->c_ctinfo;

	    /* Check if we've already seen a Content-Type header */
	    if (ct->c_ctline) {
		advise (NULL, "message %s has multiple %s: fields",
			ct->c_file, TYPE_FIELD);
		goto next_header;
	    }

	    /* Parse the Content-Type field */
	    if (get_ctinfo (hp->value, ct, 0) == NOTOK)
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
	}
	else if (!strcasecmp (hp->name, ENCODING_FIELD)) {
	/* Get Content-Transfer-Encoding field */
	    char c, *cp, *dp;
	    struct str2init *s2i;

	    /*
	     * Check if we've already seen the
	     * Content-Transfer-Encoding field
	     */
	    if (ct->c_celine) {
		advise (NULL, "message %s has multiple %s: fields",
			ct->c_file, ENCODING_FIELD);
		goto next_header;
	    }

	    /* get copy of this field */
	    ct->c_celine = cp = add (hp->value, NULL);

	    while (isspace ((unsigned char) *cp))
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
	}
	else if (!strcasecmp (hp->name, MD5_FIELD)) {
	/* Get Content-MD5 field */
	    char *cp, *dp, *ep;

	    if (!checksw)
		goto next_header;

	    if (ct->c_digested) {
		advise (NULL, "message %s has multiple %s: fields",
			ct->c_file, MD5_FIELD);
		goto next_header;
	    }

	    ep = cp = add (hp->value, NULL);	/* get a copy */

	    while (isspace ((unsigned char) *cp))
		cp++;
	    for (dp = strchr(cp, '\n'); dp; dp = strchr(dp, '\n'))
		*dp++ = ' ';
	    for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
		if (!isspace ((unsigned char) *dp))
		    break;
	    *++dp = '\0';
	    if (debugsw)
		fprintf (stderr, "%s: %s\n", MD5_FIELD, cp);

	    if (*cp == '('  &&
                get_comment (ct->c_file, MD5_FIELD, &cp, NULL) == NOTOK) {
		free (ep);
		goto out;
	    }

	    for (dp = cp; *dp && !isspace ((unsigned char) *dp); dp++)
		continue;
	    *dp = '\0';

	    readDigest (ct, cp);
	    free (ep);
	    ct->c_digested++;
	}
	else if (!strcasecmp (hp->name, ID_FIELD)) {
	/* Get Content-ID field */
	    ct->c_id = add (hp->value, ct->c_id);
	}
	else if (!strcasecmp (hp->name, DESCR_FIELD)) {
	/* Get Content-Description field */
	    ct->c_descr = add (hp->value, ct->c_descr);
	}
	else if (!strcasecmp (hp->name, DISPO_FIELD)) {
	/* Get Content-Disposition field */
	    if (get_dispo(hp->value, ct, 0) == NOTOK)
		goto out;
	}

next_header:
	hp = hp->next;	/* next header field */
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

int
add_header (CT ct, char *name, char *value)
{
    HF hp;

    /* allocate header field structure */
    hp = mh_xmalloc (sizeof(*hp));

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
 * Parse Content-Type line and (if `magic' is non-zero) mhbuild composition
 * directives.  Fills in the information of the CTinfo structure.
 */
int
get_ctinfo (char *cp, CT ct, int magic)
{
    char *dp;
    char c;
    CI ci;
    int status;

    ci = &ct->c_ctinfo;

    /* store copy of Content-Type line */
    cp = ct->c_ctline = add (cp, NULL);

    while (isspace ((unsigned char) *cp))	/* trim leading spaces */
	cp++;

    /* change newlines to spaces */
    for (dp = strchr(cp, '\n'); dp; dp = strchr(dp, '\n'))
	*dp++ = ' ';

    /* trim trailing spaces */
    for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
	if (!isspace ((unsigned char) *dp))
	    break;
    *++dp = '\0';

    if (debugsw)
	fprintf (stderr, "%s: %s\n", TYPE_FIELD, cp);

    if (*cp == '(' && get_comment (ct->c_file, TYPE_FIELD, &cp,
    				   &ci->ci_comment) == NOTOK)
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
	if (isalpha((unsigned char) *dp) && isupper ((unsigned char) *dp))
	    *dp = tolower ((unsigned char) *dp);

    while (isspace ((unsigned char) *cp))
	cp++;

    if (*cp == '(' && get_comment (ct->c_file, TYPE_FIELD, &cp,
    				   &ci->ci_comment) == NOTOK)
	return NOTOK;

    if (*cp != '/') {
	if (!magic)
	    ci->ci_subtype = add ("", NULL);
	goto magic_skip;
    }

    cp++;
    while (isspace ((unsigned char) *cp))
	cp++;

    if (*cp == '(' && get_comment (ct->c_file, TYPE_FIELD, &cp,
    				   &ci->ci_comment) == NOTOK)
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
	if (isalpha((unsigned char) *dp) && isupper ((unsigned char) *dp))
	    *dp = tolower ((unsigned char) *dp);

magic_skip:
    while (isspace ((unsigned char) *cp))
	cp++;

    if (*cp == '(' && get_comment (ct->c_file, TYPE_FIELD, &cp,
    				   &ci->ci_comment) == NOTOK)
	return NOTOK;

    if ((status = parse_header_attrs (ct->c_file, TYPE_FIELD, &cp,
    				      &ci->ci_first_pm, &ci->ci_last_pm,
				      &ci->ci_comment)) != OK) {
	return status == NOTOK ? NOTOK : OK;
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

	while (isspace ((unsigned char) *cp))
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

	while (isspace ((unsigned char) *cp))
	    cp++;
    }

    /*
     * Get any {Content-Disposition} given in buffer.
     */
    if (magic && *cp == '{') {
        ++cp;
	for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
	    if (*dp == '}')
		break;
	if (dp < cp) {
	    advise (NULL, "invalid disposition in message %s", ct->c_file);
	    ct->c_dispo = NULL;
	    return NOTOK;
	}
	
	c = *dp;
	*dp = '\0';

	if (get_dispo(cp, ct, 1) != OK)
	    return NOTOK;

	*dp++ = c;
	cp = dp;

	while (isspace ((unsigned char) *cp))
	    cp++;
    }

    /*
     * Get any extension directives (right now just the content transfer
     * encoding, but maybe others) that we care about.
     */

    if (magic && *cp == '*') {
    	/*
	 * See if it's a CTE we match on
	 */
	struct k2v *kv;

	dp = ++cp;
	while (*cp != '\0' && ! isspace((unsigned char) *cp))
	    cp++;

	if (dp == cp) {
	    advise (NULL, "invalid null transfer encoding specification");
	    return NOTOK;
	}

	if (*cp != '\0')
	    *cp++ = '\0';

	ct->c_reqencoding = CE_UNKNOWN;

	for (kv = EncodingType; kv->kv_key; kv++) {
	    if (strcasecmp(kv->kv_key, dp) == 0) {
		ct->c_reqencoding = kv->kv_value;
		break;
	    }
	}

	if (ct->c_reqencoding == CE_UNKNOWN) {
	    advise (NULL, "invalid CTE specification: \"%s\"", dp);
	    return NOTOK;
	}

	while (isspace ((unsigned char) *cp))
	    cp++;
    }

    /*
     * Check if anything is left over
     */
    if (*cp) {
        if (magic) {
	    ci->ci_magic = add (cp, NULL);

            /* If there is a Content-Disposition header and it doesn't
               have a *filename=, extract it from the magic contents.
               The r1bindex call skips any leading directory
               components. */
            if (ct->c_dispo_type &&
		!get_param(ct->c_dispo_first, "filename", '_', 1)) {
		add_param(&ct->c_dispo_first, &ct->c_dispo_last, "filename",
			  r1bindex(ci->ci_magic, '/'), 0);
	    }
        }
	else
	    advise (NULL,
		    "extraneous information in message %s's %s: field\n%*s(%s)",
                    ct->c_file, TYPE_FIELD, strlen(invo_name) + 2, "", cp);
    }

    return OK;
}


/*
 * Parse out a Content-Disposition header.  A lot of this is cribbed from
 * get_ctinfo().
 */
static int
get_dispo (char *cp, CT ct, int buildflag)
{
    char *dp, *dispoheader;
    char c;
    int status;

    /*
     * Save the whole copy of the Content-Disposition header, unless we're
     * processing a mhbuild directive.  A NULL c_dispo will be a flag to
     * mhbuild that the disposition header needs to be generated at that
     * time.
     */

    dispoheader = cp = add(cp, NULL);

    while (isspace ((unsigned char) *cp))	/* trim leading spaces */
	cp++;

    /* change newlines to spaces */
    for (dp = strchr(cp, '\n'); dp; dp = strchr(dp, '\n'))
	*dp++ = ' ';

    /* trim trailing spaces */
    for (dp = cp + strlen (cp) - 1; dp >= cp; dp--)
	if (!isspace ((unsigned char) *dp))
	    break;
    *++dp = '\0';

    if (debugsw)
	fprintf (stderr, "%s: %s\n", DISPO_FIELD, cp);

    if (*cp == '(' && get_comment (ct->c_file, DISPO_FIELD, &cp, NULL) ==
    							NOTOK) {
	free(dispoheader);
	return NOTOK;
    }

    for (dp = cp; istoken (*dp); dp++)
	continue;
    c = *dp, *dp = '\0';
    ct->c_dispo_type = add (cp, NULL);	/* store disposition type */
    *dp = c, cp = dp;

    if (*cp == '(' && get_comment (ct->c_file, DISPO_FIELD, &cp, NULL) == NOTOK)
	return NOTOK;

    if ((status = parse_header_attrs (ct->c_file, DISPO_FIELD, &cp,
    				      &ct->c_dispo_first, &ct->c_dispo_last,
				      NULL)) != OK) {
	if (status == NOTOK) {
	    free(dispoheader);
	    return NOTOK;
	}
    } else if (*cp) {
	advise (NULL,
		"extraneous information in message %s's %s: field\n%*s(%s)",
                    ct->c_file, DISPO_FIELD, strlen(invo_name) + 2, "", cp);
    }

    if (buildflag)
    	free(dispoheader);
    else
	ct->c_dispo = dispoheader;

    return OK;
}


static int
get_comment (const char *filename, const char *fieldname, char **ap,
	     char **commentp)
{
    int i;
    char *bp, *cp;
    char c, buffer[BUFSIZ], *dp;

    cp = *ap;
    bp = buffer;
    cp++;

    for (i = 0;;) {
	switch (c = *cp++) {
	case '\0':
invalid:
	advise (NULL, "invalid comment in message %s's %s: field",
		filename, fieldname);
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

    if (commentp) {
	if ((dp = *commentp)) {
	    *commentp = concat (dp, " ", buffer, NULL);
	    free (dp);
	} else {
	    *commentp = add (buffer, NULL);
	}
    }

    while (isspace ((unsigned char) *cp))
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
    NMH_UNUSED (ct);

    return OK;		/* not much to do here */
}


/*
 * TEXT
 */

static int
InitText (CT ct)
{
    char buffer[BUFSIZ];
    char *chset = NULL;
    char *cp;
    PM pm;
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

    /* scan for charset parameter */
    for (pm = ci->ci_first_pm; pm; pm = pm->pm_next)
	if (!strcasecmp (pm->pm_name, "charset"))
	    break;

    /* check if content specified a character set */
    if (pm) {
	chset = pm->pm_value;
	t->tx_charset = CHARSET_SPECIFIED;
    } else {
	t->tx_charset = CHARSET_UNSPECIFIED;
    }

    /*
     * If we can not handle character set natively,
     * then check profile for string to modify the
     * terminal or display method.
     *
     * termproc is for mhshow, though mhlist -debug prints it, too.
     */
    if (chset != NULL && !check_charset (chset, strlen (chset))) {
	snprintf (buffer, sizeof(buffer), "%s-charset-%s", invo_name, chset);
	if ((cp = context_find (buffer)))
	    ct->c_termproc = getcpy (cp);
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
    char *cp, *dp;
    PM pm;
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
    if (! skip_mp_cte_check  &&  ct->c_encoding != CE_7BIT  &&
        ct->c_encoding != CE_8BIT  &&  ct->c_encoding != CE_BINARY) {
	/* Copy the Content-Transfer-Encoding header field body so we can
	   remove any trailing whitespace and leading blanks from it. */
	char *cte = add (ct->c_celine ? ct->c_celine : "(null)", NULL);

	bp = cte + strlen (cte) - 1;
	while (bp >= cte && isspace ((unsigned char) *bp)) *bp-- = '\0';
	for (bp = cte; *bp && isblank ((unsigned char) *bp); ++bp) continue;

	admonish (NULL,
		  "\"%s/%s\" type in message %s must be encoded in\n"
		  "7bit, 8bit, or binary, per RFC 2045 (6.4).  One workaround "
		  "is to\nmanually edit the file and change the \"%s\"\n"
		  "Content-Transfer-Encoding to one of those.  For now",
		  ci->ci_type, ci->ci_subtype, ct->c_file, bp);
	free (cte);

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
    bp = 0;
    for (pm = ci->ci_first_pm; pm; pm = pm->pm_next) {
	if (!strcasecmp (pm->pm_name, "boundary")) {
	    bp = pm->pm_value;
	    break;
	}
    }

    /* complain if boundary parameter is missing */
    if (!pm) {
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
    for (cp = bp; isspace ((unsigned char) *cp); cp++)
	continue;
    if (!*cp) {
	advise (NULL, "invalid \"boundary\" parameter for \"%s/%s\" type in message %s's %s: field",
		ci->ci_type, ci->ci_subtype, ct->c_file, TYPE_FIELD);
	return NOTOK;
    }

    /* remove trailing whitespace from boundary parameter */
    for (cp = bp, dp = cp + strlen (cp) - 1; dp > cp; dp--)
	if (!isspace ((unsigned char) *dp))
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
			ct->c_subtype == MULTI_DIGEST ? -1 : 0))) {
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

    if (! suppress_bogus_mp_content_warning) {
        advise (NULL, "bogus multipart content in message %s", ct->c_file);
    }
    bogus_mp_content = 1;

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

    get_leftover_mp_content (ct, 1);
    get_leftover_mp_content (ct, 0);

    fclose (ct->c_fp);
    ct->c_fp = NULL;
    return OK;
}


/*
 * reverse the order of the parts of a multipart/alternative
 */

void
reverse_parts (CT ct)
{
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    struct part *part;
    struct part *next;

    /* Reverse the order of its parts by walking the mp_parts list
       and pushing each node to the front. */
    for (part = m->mp_parts, m->mp_parts = NULL; part; part = next) {
        next = part->mp_next;
        part->mp_next = m->mp_parts;
        m->mp_parts = part;
    }
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
		PM pm;
		struct partial *p;

		if ((p = (struct partial *) calloc (1, sizeof(*p))) == NULL)
		    adios (NULL, "out of memory");
		ct->c_ctparams = (void *) p;

		/* scan for parameters "id", "number", and "total" */
		for (pm = ci->ci_first_pm; pm; pm = pm->pm_next) {
		    if (!strcasecmp (pm->pm_name, "id")) {
			p->pm_partid = add (pm->pm_value, NULL);
			continue;
		    }
		    if (!strcasecmp (pm->pm_name, "number")) {
			if (sscanf (pm->pm_value, "%d", &p->pm_partno) != 1
			        || p->pm_partno < 1) {
invalid_param:
			    advise (NULL,
				    "invalid %s parameter for \"%s/%s\" type in message %s's %s field",
				    pm->pm_name, ci->ci_type, ci->ci_subtype,
				    ct->c_file, TYPE_FIELD);
			    return NOTOK;
			}
			continue;
		    }
		    if (!strcasecmp (pm->pm_name, "total")) {
			if (sscanf (pm->pm_value, "%d", &p->pm_maxno) != 1
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
		    ct->c_fp = NULL;
		    return NOTOK;
		}

		e->eb_parent = ct;
		e->eb_content = p;
		p->c_ctexbody = e;
		p->c_ceopenfnx = NULL;
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
		    
		    e->eb_body = bp = mh_xmalloc ((unsigned) size);
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


int
params_external (CT ct, int composing)
{
    PM pm;
    struct exbody *e = (struct exbody *) ct->c_ctparams;
    CI ci = &ct->c_ctinfo;

    ct->c_ceopenfnx = NULL;
    for (pm = ci->ci_first_pm; pm; pm = pm->pm_next) {
	if (!strcasecmp (pm->pm_name, "access-type")) {
	    struct str2init *s2i;
	    CT p = e->eb_content;

	    for (s2i = str2methods; s2i->si_key; s2i++)
		if (!strcasecmp (pm->pm_value, s2i->si_key))
		    break;
	    if (!s2i->si_key) {
		e->eb_access = pm->pm_value;
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
	if (!strcasecmp (pm->pm_name, "name")) {
	    e->eb_name = pm->pm_value;
	    continue;
	}
	if (!strcasecmp (pm->pm_name, "permission")) {
	    e->eb_permission = pm->pm_value;
	    continue;
	}
	if (!strcasecmp (pm->pm_name, "site")) {
	    e->eb_site = pm->pm_value;
	    continue;
	}
	if (!strcasecmp (pm->pm_name, "directory")) {
	    e->eb_dir = pm->pm_value;
	    continue;
	}
	if (!strcasecmp (pm->pm_name, "mode")) {
	    e->eb_mode = pm->pm_value;
	    continue;
	}
	if (!strcasecmp (pm->pm_name, "size")) {
	    sscanf (pm->pm_value, "%lu", &e->eb_size);
	    continue;
	}
	if (!strcasecmp (pm->pm_name, "server")) {
	    e->eb_server = pm->pm_value;
	    continue;
	}
	if (!strcasecmp (pm->pm_name, "subject")) {
	    e->eb_subject = pm->pm_value;
	    continue;
	}
	if (!strcasecmp (pm->pm_name, "url")) {
	    /*
	     * According to RFC 2017, we have to remove all whitespace from
	     * the URL
	     */

	    char *u, *p = pm->pm_value;
	    e->eb_url = u = mh_xmalloc(strlen(pm->pm_value) + 1);

	    for (; *p != '\0'; p++) {
	    	if (! isspace((unsigned char) *p))
		    *u++ = *p;
	    }

	    *u = '\0';
	    continue;
	}
	if (composing && !strcasecmp (pm->pm_name, "body")) {
	    e->eb_body = getcpy (pm->pm_value);
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
 * TRANSFER ENCODINGS
 */

static int
init_encoding (CT ct, OpenCEFunc openfnx)
{
    ct->c_ceopenfnx  = openfnx;
    ct->c_ceclosefnx = close_encoding;
    ct->c_cesizefnx  = size_encoding;

    return OK;
}


void
close_encoding (CT ct)
{
    CE ce = &ct->c_cefile;

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
    CE ce = &ct->c_cefile;
    struct stat st;

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
    int fd, len, skip, own_ct_fp = 0, text = ct->c_type == CT_TEXT;
    uint32_t bits;
    unsigned char value, b;
    char *cp, *ep, buffer[BUFSIZ];
    /* sbeck -- handle suffixes */
    CI ci;
    CE ce = &ct->c_cefile;
    MD5_CTX mdContext;

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
	ce->ce_unlink = 1;
    } else {
	ce->ce_file = add (*file, NULL);
	ce->ce_unlink = 0;
    }

    /* sbeck@cise.ufl.edu -- handle suffixes */
    ci = &ct->c_ctinfo;
    snprintf (buffer, sizeof(buffer), "%s-suffix-%s/%s",
              invo_name, ci->ci_type, ci->ci_subtype);
    cp = context_find (buffer);
    if (cp == NULL || *cp == '\0') {
        snprintf (buffer, sizeof(buffer), "%s-suffix-%s", invo_name,
                  ci->ci_type);
        cp = context_find (buffer);
    }
    if (cp != NULL && *cp != '\0') {
	if (ce->ce_unlink) {
	    /* Create temporary file with filename extension. */
	    if ((ce->ce_file = m_mktemps(invo_name, cp, NULL, NULL)) == NULL) {
		adios(NULL, "unable to create temporary file in %s",
		      get_temp_dir());
	    }
	} else {
	    ce->ce_file = add (cp, ce->ce_file);
	}
    } else if (*file == NULL) {
	char *tempfile;
	if ((tempfile = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
	    adios(NULL, "unable to create temporary file in %s",
		  get_temp_dir());
	}
	ce->ce_file = add (tempfile, NULL);
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

    if ((len = ct->c_end - ct->c_begin) < 0)
	adios (NULL, "internal error(1)");

    if (! ct->c_fp) {
	if ((ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
	    content_error (ct->c_file, ct, "unable to open for reading");
	    return NOTOK;
	}
	own_ct_fp = 1;
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
		    if (isspace ((unsigned char) *cp))
			break;
		    if (skip || (((unsigned char) *cp) & 0x80)
			|| (value = b642nib[((unsigned char) *cp) & 0x7f]) > 0x3f) {
			if (debugsw) {
			    fprintf (stderr, "*cp=0x%x pos=%ld skip=%d\n",
				(unsigned char) *cp,
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
		    	b = (bits >> 16) & 0xff;
			if (!text || b != '\r')
			    putc ((char) b, ce->ce_fp);
			if (digested)
			    MD5Update (&mdContext, &b, 1);
			if (skip < 2) {
			    b = (bits >> 8) & 0xff;
			    if (! text || b != '\r')
				putc ((char) b, ce->ce_fp);
			    if (digested)
				MD5Update (&mdContext, &b, 1);
			    if (skip < 1) {
			    	b = bits & 0xff;
				if (! text || b != '\r')
				    putc ((char) b, ce->ce_fp);
				if (digested)
				    MD5Update (&mdContext, &b, 1);
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
    if (own_ct_fp) {
      fclose (ct->c_fp);
      ct->c_fp = NULL;
    }
    return fileno (ce->ce_fp);

clean_up:
    if (own_ct_fp) {
      fclose (ct->c_fp);
      ct->c_fp = NULL;
    }
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
    int	cc, digested, len, quoted, own_ct_fp = 0;
    char *cp, *ep;
    char buffer[BUFSIZ];
    unsigned char mask;
    CE ce = &ct->c_cefile;
    /* sbeck -- handle suffixes */
    CI ci;
    MD5_CTX mdContext;

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
	ce->ce_unlink = 1;
    } else {
	ce->ce_file = add (*file, NULL);
	ce->ce_unlink = 0;
    }

    /* sbeck@cise.ufl.edu -- handle suffixes */
    ci = &ct->c_ctinfo;
    snprintf (buffer, sizeof(buffer), "%s-suffix-%s/%s",
              invo_name, ci->ci_type, ci->ci_subtype);
    cp = context_find (buffer);
    if (cp == NULL || *cp == '\0') {
        snprintf (buffer, sizeof(buffer), "%s-suffix-%s", invo_name,
                  ci->ci_type);
        cp = context_find (buffer);
    }
    if (cp != NULL && *cp != '\0') {
	if (ce->ce_unlink) {
	    /* Create temporary file with filename extension. */
	    if ((ce->ce_file = m_mktemps(invo_name, cp, NULL, NULL)) == NULL) {
		adios(NULL, "unable to create temporary file in %s",
		      get_temp_dir());
	    }
	} else {
	    ce->ce_file = add (cp, ce->ce_file);
	}
    } else if (*file == NULL) {
	char *tempfile;
	if ((tempfile = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
	    adios(NULL, "unable to create temporary file in %s",
		  get_temp_dir());
	}
	ce->ce_file = add (tempfile, NULL);
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

    if ((len = ct->c_end - ct->c_begin) < 0)
	adios (NULL, "internal error(2)");

    if (! ct->c_fp) {
	if ((ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
	    content_error (ct->c_file, ct, "unable to open for reading");
	    return NOTOK;
	}
	own_ct_fp = 1;
    }

    if ((digested = ct->c_digested))
	MD5Init (&mdContext);

    quoted = 0;
#ifdef lint
    mask = 0;
#endif

    fseek (ct->c_fp, ct->c_begin, SEEK_SET);
    while (len > 0) {
	if (fgets (buffer, sizeof(buffer) - 1, ct->c_fp) == NULL) {
	    content_error (NULL, ct, "premature eof");
	    goto clean_up;
	}

	if ((cc = strlen (buffer)) > len)
	    cc = len;
	len -= cc;

	for (ep = (cp = buffer) + cc - 1; cp <= ep; ep--)
	    if (!isspace ((unsigned char) *ep))
		break;
	*++ep = '\n', ep++;

	for (; cp < ep; cp++) {
	    if (quoted > 0) {
		/* in an escape sequence */
		if (quoted == 1) {
		    /* at byte 1 of an escape sequence */
		    mask = hex2nib[((unsigned char) *cp) & 0x7f];
		    /* next is byte 2 */
		    quoted = 2;
		} else {
		    /* at byte 2 of an escape sequence */
		    mask <<= 4;
		    mask |= hex2nib[((unsigned char) *cp) & 0x7f];
		    putc (mask, ce->ce_fp);
		    if (digested)
			MD5Update (&mdContext, &mask, 1);
		    if (ferror (ce->ce_fp)) {
			content_error (ce->ce_file, ct, "error writing to");
			goto clean_up;
		    }
		    /* finished escape sequence; next may be literal or a new
		     * escape sequence */
		    quoted = 0;
		}
		/* on to next byte */
		continue;
	    }

	    /* not in an escape sequence */
	    if (*cp == '=') {
		/* starting an escape sequence, or invalid '='? */
		if (cp + 1 < ep && cp[1] == '\n') {
		    /* "=\n" soft line break, eat the \n */
		    cp++;
		    continue;
		}
		if (cp + 1 >= ep || cp + 2 >= ep) {
		    /* We don't have 2 bytes left, so this is an invalid
		     * escape sequence; just show the raw bytes (below). */
		} else if (isxdigit ((unsigned char) cp[1]) &&
					isxdigit ((unsigned char) cp[2])) {
		    /* Next 2 bytes are hex digits, making this a valid escape
		     * sequence; let's decode it (above). */
		    quoted = 1;
		    continue;
		} else {
		    /* One or both of the next 2 is out of range, making this
		     * an invalid escape sequence; just show the raw bytes
		     * (below). */
		}
	    }

	    /* Just show the raw byte. */
	    putc (*cp, ce->ce_fp);
	    if (digested) {
		if (*cp == '\n') {
		    MD5Update (&mdContext, (unsigned char *) "\r\n",2);
		} else {
		    MD5Update (&mdContext, (unsigned char *) cp, 1);
		}
	    }
	    if (ferror (ce->ce_fp)) {
		content_error (ce->ce_file, ct, "error writing to");
		goto clean_up;
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
    if (own_ct_fp) {
      fclose (ct->c_fp);
      ct->c_fp = NULL;
    }
    return fileno (ce->ce_fp);

clean_up:
    free_encoding (ct, 0);
    if (own_ct_fp) {
      fclose (ct->c_fp);
      ct->c_fp = NULL;
    }
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


int
open7Bit (CT ct, char **file)
{
    int	cc, fd, len, own_ct_fp = 0;
    char buffer[BUFSIZ];
    /* sbeck -- handle suffixes */
    char *cp;
    CI ci;
    CE ce = &ct->c_cefile;

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
	ce->ce_unlink = 1;
    } else {
	ce->ce_file = add (*file, NULL);
	ce->ce_unlink = 0;
    }

    /* sbeck@cise.ufl.edu -- handle suffixes */
    ci = &ct->c_ctinfo;
    snprintf (buffer, sizeof(buffer), "%s-suffix-%s/%s",
              invo_name, ci->ci_type, ci->ci_subtype);
    cp = context_find (buffer);
    if (cp == NULL || *cp == '\0') {
        snprintf (buffer, sizeof(buffer), "%s-suffix-%s", invo_name,
                  ci->ci_type);
        cp = context_find (buffer);
    }
    if (cp != NULL && *cp != '\0') {
	if (ce->ce_unlink) {
	    /* Create temporary file with filename extension. */
	    if ((ce->ce_file = m_mktemps(invo_name, cp, NULL, NULL)) == NULL) {
		adios(NULL, "unable to create temporary file in %s",
		      get_temp_dir());
	    }
	} else {
	    ce->ce_file = add (cp, ce->ce_file);
	}
    } else if (*file == NULL) {
	char *tempfile;
	if ((tempfile = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
	    adios(NULL, "unable to create temporary file in %s",
		  get_temp_dir());
	}
	ce->ce_file = add (tempfile, NULL);
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

    if (ct->c_type == CT_MULTIPART) {
	CI ci = &ct->c_ctinfo;
	char *buffer;

	len = 0;
	fprintf (ce->ce_fp, "%s: %s/%s", TYPE_FIELD, ci->ci_type, ci->ci_subtype);
	len += strlen (TYPE_FIELD) + 2 + strlen (ci->ci_type)
	    + 1 + strlen (ci->ci_subtype);
	buffer = output_params(len, ci->ci_first_pm, &len, 0);

	if (buffer) {
	    fputs (buffer, ce->ce_fp);
	    free(buffer);
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
	if (ct->c_dispo)
	    fprintf (ce->ce_fp, "%s:%s", DISPO_FIELD, ct->c_dispo);
	fprintf (ce->ce_fp, "\n");
    }

    if ((len = ct->c_end - ct->c_begin) < 0)
	adios (NULL, "internal error(3)");

    if (! ct->c_fp) {
	if ((ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
	    content_error (ct->c_file, ct, "unable to open for reading");
	    return NOTOK;
	}
	own_ct_fp = 1;
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
    if (own_ct_fp) {
      fclose (ct->c_fp);
      ct->c_fp = NULL;
    }
    return fileno (ce->ce_fp);

clean_up:
    free_encoding (ct, 0);
    if (own_ct_fp) {
      fclose (ct->c_fp);
      ct->c_fp = NULL;
    }
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
    CE ce = &ct->c_cefile;

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
		(void) m_unlink (cachefile);
	    }
	    else
		if (ferror (fp)) {
		    admonish (cachefile, "error writing");
		    (void) m_unlink (cachefile);
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
    CE ce = &ct->c_cefile;
    static char *username = NULL;
    static char *password = NULL;

    e  = ct->c_ctexbody;

    if ((ftp = context_find (nmhaccessftp)) && !*ftp)
	ftp = NULL;

    if (!ftp)
	return NOTOK;

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
	snprintf (buffer, sizeof(buffer), "%s@%s", getusername (),
		  LocalName (1));
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
    else {
	char *tempfile;
	if ((tempfile = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
	    adios(NULL, "unable to create temporary file in %s",
		  get_temp_dir());
	}
	ce->ce_file = add (tempfile, NULL);
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

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

	for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
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
		    username = password = NULL;
		    ce->ce_unlink = 1;
		    return NOTOK;
		}
		break;
	}
    }

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
		    (void) m_unlink (cachefile);
		}
		else
		    if (ferror (fp)) {
			admonish (cachefile, "error writing");
			(void) m_unlink (cachefile);
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
    CE ce = &ct->c_cefile;

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

    /* Now, construct query message */
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

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
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
	char *tempfile;
	if ((tempfile = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
	    adios(NULL, "unable to create temporary file in %s",
		  get_temp_dir());
	}
	ce->ce_file = add (tempfile, NULL);
	ce->ce_unlink = 1;
    } else {
	ce->ce_file = add (*file, NULL);
	ce->ce_unlink = 0;
    }

    if ((ce->ce_fp = fopen (ce->ce_file, "w+")) == NULL) {
	content_error (ce->ce_file, ct, "unable to fopen for reading/writing");
	return NOTOK;
    }

    /* showproc is for mhshow and mhstore, though mhlist -debug
     * prints it, too. */
    if (ct->c_showproc)
	free (ct->c_showproc);
    ct->c_showproc = add ("true", NULL);

    fseek (ce->ce_fp, 0L, SEEK_SET);
    *file = ce->ce_file;
    return fileno (ce->ce_fp);
}


/*
 * URL
 */

static int
InitURL (CT ct)
{
    return init_encoding (ct, openURL);
}


static int
openURL (CT ct, char **file)
{
    struct exbody *e = ct->c_ctexbody;
    CE ce = &ct->c_cefile;
    char *urlprog, *program;
    char buffer[BUFSIZ], cachefile[BUFSIZ];
    int fd, caching, cachetype;
    struct msgs_array args = { 0, 0, NULL};
    pid_t child_id;

    if ((urlprog = context_find(nmhaccessurl)) && *urlprog == '\0')
    	urlprog = NULL;

    if (! urlprog) {
    	content_error(NULL, ct, "No entry for nmh-access-url in profile");
    	return NOTOK;
    }

    switch (openExternal(e->eb_parent, e->eb_content, ce, file, &fd)) {
    	case NOTOK:
	    return NOTOK;

	case OK:
	    break;

	case DONE:
	    return fd;
    }

    if (!e->eb_url) {
    	content_error(NULL, ct, "missing url parameter");
	return NOTOK;
    }

    if (xpid) {
	if (xpid < 0)
	    xpid = -xpid;
	pidcheck (pidwait (xpid, NOTOK));
	xpid = 0;
    }

    ce->ce_unlink = (*file == NULL);
    caching = 0;
    cachefile[0] = '\0';

    if (find_cache(NULL, wcachesw, &cachetype, e->eb_content->c_id,
    		   cachefile, sizeof(cachefile)) != NOTOK) {
	if (*file == NULL) {
	    ce->ce_unlink = 0;
	    caching = 1;
	}
    }

    if (*file)
    	ce->ce_file = add(*file, NULL);
    else if (caching)
    	ce->ce_file = add(cachefile, NULL);
    else {
	char *tempfile;
	if ((tempfile = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
	    adios(NULL, "unable to create temporary file in %s",
		  get_temp_dir());
	}
	ce->ce_file = add (tempfile, NULL);
    }

    if ((ce->ce_fp = fopen(ce->ce_file, "w+")) == NULL) {
    	content_error(ce->ce_file, ct, "unable to fopen for read/writing");
	return NOTOK;
    }

    switch (child_id = fork()) {
    case NOTOK:
    	adios ("fork", "unable to");
	/* NOTREACHED */

    case OK:
    	argsplit_msgarg(&args, urlprog, &program);
	app_msgarg(&args, e->eb_url);
	app_msgarg(&args, NULL);
	dup2(fileno(ce->ce_fp), 1);
	close(fileno(ce->ce_fp));
	execvp(program, args.msgs);
	fprintf(stderr, "Unable to exec ");
	perror(program);
	_exit(-1);
	/* NOTREACHED */

    default:
    	if (pidXwait(child_id, NULL)) {
	    ce->ce_unlink = 1;
	    return NOTOK;
	}
    }

    if (cachefile[0]) {
    	if (caching)
	    chmod(cachefile, cachetype ? m_gmprot() : 0444);
	else {
	    int mask;
	    FILE *fp;

	    mask = umask (cachetype ? ~m_gmprot() : 0222);
	    if ((fp = fopen(cachefile, "w"))) {
	    	int cc;
		FILE *gp = ce->ce_fp;

		fseeko(gp, 0, SEEK_SET);

		while ((cc = fread(buffer, sizeof(*buffer),
				   sizeof(buffer), gp)) > 0)
		    fwrite(buffer, sizeof(*buffer), cc, fp);

		fflush(fp);

		if (ferror(gp)) {
		    admonish(ce->ce_file, "error reading");
		    (void) m_unlink (cachefile);
		}
	    }
	    umask(mask);
	}
    }

    fseeko(ce->ce_fp, 0, SEEK_SET);
    *file = ce->ce_file;
    return fd;
}

static int
readDigest (CT ct, char *cp)
{
    int	bitno, skip;
    uint32_t bits;
    char *bp = cp;
    unsigned char *dp, value, *ep;

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
		    *dp++ = (bits >> 16) & 0xff;
		    if (skip < 2) {
			*dp++ = (bits >> 8) & 0xff;
			if (skip < 1)
			    *dp++ = bits & 0xff;
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
		     (int)(cp - bp));
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


/* Multipart parts might have content before the first subpart and/or
   after the last subpart that hasn't been stored anywhere else, so do
   that. */
int
get_leftover_mp_content (CT ct, int before /* or after */) {
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    char *boundary;
    int found_boundary = 0;
    char buffer[BUFSIZ];
    int max = BUFSIZ;
    int read = 0;
    char *content = NULL;

    if (! m) return NOTOK;

    if (before) {
        if (! m->mp_parts  ||  ! m->mp_parts->mp_part) return NOTOK;

        /* Isolate the beginning of this part to the beginning of the
           first subpart and save any content between them. */
        fseeko (ct->c_fp, ct->c_begin, SEEK_SET);
        max = m->mp_parts->mp_part->c_begin - ct->c_begin;
        boundary = concat ("--", m->mp_start, NULL);
    } else {
        struct part *last_subpart = NULL;
        struct part *subpart;

        /* Go to the last subpart to get its end position. */
        for (subpart = m->mp_parts; subpart; subpart = subpart->mp_next) {
            last_subpart = subpart;
        }

        if (last_subpart == NULL) return NOTOK;

        /* Isolate the end of the last subpart to the end of this part
           and save any content between them. */
        fseeko (ct->c_fp, last_subpart->mp_part->c_end, SEEK_SET);
        max = ct->c_end - last_subpart->mp_part->c_end;
        boundary = concat ("--", m->mp_stop, NULL);
    }

    /* Back up by 1 to pick up the newline. */
    while (fgets (buffer, sizeof(buffer) - 1, ct->c_fp)) {
        read += strlen (buffer);
        /* Don't look beyond beginning of first subpart (before) or
           next part (after). */
        if (read > max) buffer[read-max] = '\0';

        if (before) {
            if (! strcmp (buffer, boundary)) {
                found_boundary = 1;
            }
        } else {
            if (! found_boundary  &&  ! strcmp (buffer, boundary)) {
                found_boundary = 1;
                continue;
            }
        }

        if ((before && ! found_boundary)  ||  (! before && found_boundary)) {
            if (content) {
                char *old_content = content;
                content = concat (content, buffer, NULL);
                free (old_content);
            } else {
                content = before
                    ?  concat ("\n", buffer, NULL)
                    :  concat (buffer, NULL);
            }
        }

        if (before) {
            if (found_boundary  ||  read > max) break;
        } else {
            if (read > max) break;
        }
    }

    /* Skip the newline if that's all there is. */
    if (content) {
        char *cp;

        /* Remove trailing newline, except at EOF. */
        if ((before || ! feof (ct->c_fp)) &&
            (cp = content + strlen (content)) > content  &&
            *--cp == '\n') {
            *cp = '\0';
        }

        if (strlen (content) > 1) {
            if (before) {
                m->mp_content_before = content;
            } else {
                m->mp_content_after = content;
            }
        } else {
            free (content);
        }
    }

    free (boundary);

    return OK;
}


char *
ct_type_str (int type) {
    switch (type) {
    case CT_APPLICATION:
        return "application";
    case CT_AUDIO:
        return "audio";
    case CT_IMAGE:
        return "image";
    case CT_MESSAGE:
        return "message";
    case CT_MULTIPART:
        return "multipart";
    case CT_TEXT:
        return "text";
    case CT_VIDEO:
        return "video";
    case CT_EXTENSION:
        return "extension";
    default:
        return "unknown_type";
    }
}


char *
ct_subtype_str (int type, int subtype) {
    switch (type) {
    case CT_APPLICATION:
        switch (subtype) {
        case APPLICATION_OCTETS:
            return "octets";
        case APPLICATION_POSTSCRIPT:
            return "postscript";
        default:
            return "unknown_app_subtype";
        }
    case CT_MESSAGE:
        switch (subtype) {
        case MESSAGE_RFC822:
            return "rfc822";
        case MESSAGE_PARTIAL:
            return "partial";
        case MESSAGE_EXTERNAL:
            return "external";
        default:
            return "unknown_msg_subtype";
        }
    case CT_MULTIPART:
        switch (subtype) {
        case MULTI_MIXED:
            return "mixed";
        case MULTI_ALTERNATE:
            return "alternative";
        case MULTI_DIGEST:
            return "digest";
        case MULTI_PARALLEL:
            return "parallel";
        default:
            return "unknown_multipart_subtype";
        }
    case CT_TEXT:
        switch (subtype) {
        case TEXT_PLAIN:
            return "plain";
        case TEXT_RICHTEXT:
            return "richtext";
        case TEXT_ENRICHED:
            return "enriched";
        default:
            return "unknown_text_subtype";
        }
    default:
        return "unknown_type";
    }
}


/* Find the content type and InitFunc for the CT. */
const struct str2init *
get_ct_init (int type) {
    const struct str2init *sp;

    for (sp = str2cts; sp->si_key; ++sp) {
        if (type == sp->si_val) {
            return sp;
        }
    }

    return NULL;
}

const char *
ce_str (int encoding) {
    switch (encoding) {
    case CE_BASE64:
        return "base64";
    case CE_QUOTED:
        return "quoted-printable";
    case CE_8BIT:
        return "8bit";
    case CE_7BIT:
        return "7bit";
    case CE_BINARY:
        return "binary";
    case CE_EXTENSION:
        return "extension";
    case CE_EXTERNAL:
        return "external";
    default:
        return "unknown";
    }
}

/* Find the content type and InitFunc for the content encoding method. */
const struct str2init *
get_ce_method (const char *method) {
    struct str2init *sp;

    for (sp = str2ces; sp->si_key; ++sp) {
        if (! strcasecmp (method, sp->si_key)) {
            return sp;
        }
    }

    return NULL;
}

/*
 * Parse a series of MIME attributes (or parameters) given a header as
 * input.
 *
 * Arguments include:
 *
 * filename	- Name of input file (for error messages)
 * fieldname	- Name of field being processed
 * headerp	- Pointer to pointer of the beginning of the MIME attributes.
 *		  Updated to point to end of attributes when finished.
 * param_head	- Pointer to head of parameter list
 * param_tail	- Pointer to tail of parameter list
 * commentp	- Pointer to header comment pointer (may be NULL)
 *
 * Returns OK if parsing was successful, NOTOK if parsing failed, and
 * DONE to indicate a benign error (minor parsing error, but the program
 * should continue).
 */

static int
parse_header_attrs (const char *filename, const char *fieldname,
		    char **header_attrp, PM *param_head, PM *param_tail,
		    char **commentp)
{
    char *cp = *header_attrp;
    PM pm;
    struct sectlist {
	char *value;
	int index;
	int len;
	struct sectlist *next;
    } *sp, *sp2;
    struct parmlist {
	char *name;
	char *charset;
	char *lang;
	struct sectlist *sechead;
	struct parmlist *next;
    } *pp, *pp2, *phead = NULL;

    while (*cp == ';') {
	char *dp, *vp, *up, *nameptr, *valptr, *charset = NULL, *lang = NULL;
	int encoded = 0, partial = 0, len = 0, index = 0;

	cp++;
	while (isspace ((unsigned char) *cp))
	    cp++;

	if (*cp == '('  &&
            get_comment (filename, fieldname, &cp, commentp) == NOTOK) {
	    return NOTOK;
        }

	if (*cp == 0) {
	    advise (NULL,
		    "extraneous trailing ';' in message %s's %s: "
                    "parameter list",
		    filename, fieldname);
	    return DONE;
	}

	/* down case the attribute name */
	for (dp = cp; istoken ((unsigned char) *dp); dp++)
	    if (isalpha((unsigned char) *dp) && isupper ((unsigned char) *dp))
		*dp = tolower ((unsigned char) *dp);

	for (up = dp; isspace ((unsigned char) *dp);)
	    dp++;
	if (dp == cp || *dp != '=') {
	    advise (NULL,
		    "invalid parameter in message %s's %s: "
                    "field\n%*sparameter %s (error detected at offset %d)",
		    filename, fieldname, strlen(invo_name) + 2, "",cp, dp - cp);
	    return NOTOK;
	}

	/*
	 * To handle RFC 2231, we have to deal with the following extensions:
	 *
	 * name*=encoded-value
	 * name*<N>=part-N-of-a-parameter-value
	 * name*<N>*=encoded-part-N-of-a-parameter-value
	 *
	 * So the rule is:
	 * If there's a * right before the equal sign, it's encoded.
	 * If there's a * and one or more digits, then it's section N.
	 *
	 * Remember we can have one or the other, or both.  cp points to
	 * beginning of name, up points past the last character in the
	 * parameter name.
	 */

	for (vp = cp; vp < up; vp++) {
	    if (*vp == '*' && vp < up - 1) {
		partial = 1;
		continue;
	    } else if (*vp == '*' && vp == up - 1) {
	    	encoded = 1;
	    } else if (partial) {
		if (isdigit((unsigned char) *vp))
		    index = *vp - '0' + index * 10;
		else {
		    advise (NULL, "invalid parameter index in message %s's "
			    "%s: field\n%*s(parameter %s)", filename,
			    fieldname, strlen(invo_name) + 2, "", cp);
		    return NOTOK;
		}
	    } else {
		len++;
	    }
	}

	/*
	 * Break out the parameter name and value sections and allocate
	 * memory for each.
	 */

	nameptr = mh_xmalloc(len + 1);
	strncpy(nameptr, cp, len);
	nameptr[len] = '\0';

	for (dp++; isspace ((unsigned char) *dp);)
	    dp++;

	if (encoded) {
	    /*
	     * Single quotes delimit the character set and language tag.
	     * They are required on the first section (or a complete
	     * parameter).
	     */
	    if (index == 0) {
	    	vp = dp;
		while (*vp != '\'' && !isspace((unsigned char) *vp) &&
							*vp != '\0')
		    vp++;
		if (*vp == '\'') {
		    if (vp != dp) {
			len = vp - dp;
			charset = mh_xmalloc(len + 1);
			strncpy(charset, dp, len);
			charset[len] = '\0';
		    } else {
			charset = NULL;
		    }
		    vp++;
		} else {
		    advise(NULL, "missing charset in message %s's %s: "
			   "field\n%*s(parameter %s)", filename, fieldname,
			   strlen(invo_name) + 2, "", nameptr);
		    free(nameptr);
		    return NOTOK;
		}
		dp = vp;

		while (*vp != '\'' && !isspace((unsigned char) *vp) &&
							*vp != '\0')
		    vp++;

		if (*vp == '\'') {
		    if (vp != dp) {
			len = vp - dp;
			lang = mh_xmalloc(len + 1);
			strncpy(lang, dp, len);
			lang[len] = '\0';
		    } else {
			lang = NULL;
		    }
		    vp++;
		} else {
		    advise(NULL, "missing language tag in message %s's %s: "
			   "field\n%*s(parameter %s)", filename, fieldname,
			   strlen(invo_name) + 2, "", nameptr);
		    free(nameptr);
		    if (charset)
			free(charset);
		    return NOTOK;
		}

		dp = vp;
	    }

	    /*
	     * At this point vp should be pointing at the beginning
	     * of the encoded value/section.  Continue until we reach
	     * the end or get whitespace.  But first, calculate the
	     * length so we can allocate the correct buffer size.
	     */

	    for (vp = dp, len = 0; istoken(*vp); vp++) {
		if (*vp == '%') {
		     if (*(vp + 1) == '\0' ||
				!isxdigit((unsigned char) *(vp + 1)) ||
				*(vp + 2) == '\0' ||
				!isxdigit((unsigned char) *(vp + 2))) {
			advise(NULL, "invalid encoded sequence in message "
			       "%s's %s: field\n%*s(parameter %s)",
			       filename, fieldname, strlen(invo_name) + 2,
			       "", nameptr);
			free(nameptr);
			if (charset)
			    free(charset);
			if (lang)
			    free(lang);
			return NOTOK;
		    }
		    vp += 2;
		}
		len++;
	    }

	    up = valptr = mh_xmalloc(len + 1);

	    for (vp = dp; istoken(*vp); vp++) {
		if (*vp == '%') {
		    *up++ = decode_qp(*(vp + 1), *(vp + 2));
		    vp += 2;
		} else {
		    *up++ = *vp;
		}
	    }

	    *up = '\0';
	    cp = vp;
	} else {
	    /*
	     * A "normal" string.  If it's got a leading quote, then we
	     * strip the quotes out.  Otherwise go until we reach the end
	     * or get whitespace.  Note we scan it twice; once to get the
	     * length, then the second time copies it into the destination
	     * buffer.
	     */

	    len = 0;

	    if (*dp == '"') {
		for (cp = dp + 1;;) {
		    switch (*cp++) {
		    case '\0':
bad_quote:
		        advise (NULL,
				"invalid quoted-string in message %s's %s: "
                                "field\n%*s(parameter %s)",
				filename, fieldname, strlen(invo_name) + 2, "",
				nameptr);
			free(nameptr);
			if (charset)
			    free(charset);
			if (lang)
			    free(lang);
			return NOTOK;
		    case '"':
			break;

		    case '\\':
			if (*++cp == '\0')
			    goto bad_quote;
			/* FALL THROUGH */
		    default:
			len++;
			continue;
		    }
		    break;
		}

	    } else {
		for (cp = dp; istoken (*cp); cp++) {
		    len++;
		}
	    }

	    valptr = mh_xmalloc(len + 1);

	    if (*dp == '"') {
	    	int i;
		for (cp = dp + 1, vp = valptr, i = 0; i < len; i++) {
		    if (*cp == '\\') {
			cp++;
		    }
		    *vp++ = *cp++;
		}
		cp++;
	    } else {
	    	strncpy(valptr, cp = dp, len);
		cp += len;
	    }

	    valptr[len] = '\0';
	}

	/*
	 * If 'partial' is set, we don't allocate a parameter now.  We
	 * put it on the parameter linked list to be reassembled later.
	 *
	 * "phead" points to a list of all parameters we need to reassemble.
	 * Each parameter has a list of sections. We insert the sections in
	 * order.
	 */

	if (partial) {
	    for (pp = phead; pp != NULL; pp = pp->next) {
		if (strcasecmp(nameptr, pp->name) == 0)
		    break;
	    }

	    if (pp == NULL) {
		pp = mh_xmalloc(sizeof(*pp));
		memset(pp, 0, sizeof(*pp));
		pp->name = nameptr;
		pp->next = phead;
		phead = pp;
	    }

	    /*
	     * Insert this into the section linked list
	     */

	    sp = mh_xmalloc(sizeof(*sp));
	    memset(sp, 0, sizeof(*sp));
	    sp->value = valptr;
	    sp->index = index;
	    sp->len = len;

	    if (pp->sechead == NULL || pp->sechead->index > index) {
		sp->next = pp->sechead;
		pp->sechead = sp;
	    } else {
		for (sp2 = pp->sechead; sp2 != NULL; sp2 = sp2->next) {
		    if (sp2->index == sp->index) {
			advise (NULL, "duplicate index (%d) in message "
				"%s's %s: field\n%*s(parameter %s)", sp->index,
				filename, fieldname, strlen(invo_name) + 2, "",
				nameptr);
			return NOTOK;
		    }
		    if (sp2->index < sp->index &&
			(sp2->next == NULL || sp2->next->index > sp->index)) {
			sp->next = sp2->next;
			sp2->next = sp;
			break;
		    }
		}

		if (sp2 == NULL) {
		    advise(NULL, "Internal error: cannot insert partial "
		    	   "param in message %s's %s: field\n%*s(parameter %s)",
			   filename, fieldname, strlen(invo_name) + 2, "",
			   nameptr);
		    return NOTOK;
		}
	    }

	    /*
	     * Save our charset and lang tags.
	     */

	    if (index == 0 && encoded) {
		if (pp->charset)
		    free(pp->charset);
	    	pp->charset = charset;
		if (pp->lang)
		    free(pp->lang);
		pp->lang = lang;
	    }
	} else {
	    pm = add_param(param_head, param_tail, nameptr, valptr, 1);
	    pm->pm_charset = charset;
	    pm->pm_lang = lang;
	}

	while (isspace ((unsigned char) *cp))
	    cp++;

	if (*cp == '('  &&
            get_comment (filename, fieldname, &cp, commentp) == NOTOK) {
	    return NOTOK;
        }
    }

    /*
     * Now that we're done, reassemble all of the partial parameters.
     */

    for (pp = phead; pp != NULL; ) {
    	char *p, *q;
	size_t tlen = 0;
	int pindex = 0;
	for (sp = pp->sechead; sp != NULL; sp = sp->next) {
	    if (sp->index != pindex++) {
		advise(NULL, "missing section %d for parameter in "
		       "message %s's %s: field\n%*s(parameter %s)", pindex - 1,
		       filename, fieldname, strlen(invo_name) + 2, "",
		       pp->name);
		return NOTOK;
	    }
	    tlen += sp->len;
	}

	p = q = mh_xmalloc(tlen + 1);
	for (sp = pp->sechead; sp != NULL; ) {
	    memcpy(q, sp->value, sp->len);
	    q += sp->len;
	    free(sp->value);
	    sp2 = sp->next;
	    free(sp);
	    sp = sp2;
	}

	p[tlen] = '\0';

	pm = add_param(param_head, param_tail, pp->name, p, 1);
	pm->pm_charset = pp->charset;
	pm->pm_lang = pp->lang;
	pp2 = pp->next;
	free(pp);
	pp = pp2;
    }

    *header_attrp = cp;
    return OK;
}

/*
 * Return the charset for a particular content type.  Return pointer is
 * only valid until the next call to content_charset().
 */

char *
content_charset (CT ct) {
    static char *ret_charset = NULL;

    if (ret_charset != NULL) {
	free(ret_charset);
    }

    ret_charset = get_param(ct->c_ctinfo.ci_first_pm, "charset", '?', 0);

    return ret_charset ? ret_charset : "US-ASCII";
}


/*
 * Create a string based on a list of output parameters.  Assume that this
 * parameter string will be appended to an existing header, so start out
 * with the separator (;).  Perform RFC 2231 encoding when necessary.
 */

char *
output_params(size_t initialwidth, PM params, int *offsetout, int external)
{
    char *paramout = NULL;
    char line[CPERLIN * 2], *q;
    int curlen, index, cont, encode, i;
    size_t valoff, numchars;

    while (params != NULL) {
	encode = 0;
	index = 0;
	valoff = 0;
	q = line;

	if (external && strcasecmp(params->pm_name, "body") == 0)
	    continue;

	if (strlen(params->pm_name) > CPERLIN) {
	    advise(NULL, "Parameter name \"%s\" is too long", params->pm_name);
	    if (paramout)
		free(paramout);
	    return NULL;
	}

	curlen = param_len(params, index, valoff, &encode, &cont, &numchars);

	/*
	 * Loop until we get a parameter that fits within a line.  We
	 * assume new lines start with a tab, so check our overflow based
	 * on that.
	 */

	while (cont) {
	    *q++ = ';';
	    *q++ = '\n';
	    *q++ = '\t';

	    /*
	     * At this point we're definitely continuing the line, so
	     * be sure to include the parameter name and section index.
	     */

	    q += snprintf(q, sizeof(line) - (q - line), "%s*%d",
			  params->pm_name, index);

	    /*
	     * Both of these functions do a NUL termination
	     */

	    if (encode)
		i = encode_param(params, q, sizeof(line) - (q - line),
				 numchars, valoff, index);
	    else
		i = normal_param(params, q, sizeof(line) - (q - line),
				 numchars, valoff);

	    if (i == 0) {
		if (paramout)
		    free(paramout);
		return NULL;
	    }

	    valoff += numchars;
	    index++;
	    curlen = param_len(params, index, valoff, &encode, &cont,
			       &numchars);
	    q = line;

	    /*
	     * "line" starts with a ;\n\t, so that doesn't count against
	     * the length.  But add 8 since it starts with a tab; that's
	     * how we end up with 5.
	     */

	    initialwidth = strlen(line) + 5;

	    /*
	     * At this point the line should be built, so add it to our
	     * current output buffer.
	     */

	    paramout = add(line, paramout);
	}

	/*
	 * If this won't fit on the line, start a new one.  Save room in
	 * case we need a semicolon on the end
	 */

	if (initialwidth + curlen > CPERLIN - 1) {
	    *q++ = ';';
	    *q++ = '\n';
	    *q++ = '\t';
	    initialwidth = 8;
	} else {
	    *q++ = ';';
	    *q++ = ' ';
	    initialwidth += 2;
	}

	/*
	 * At this point, we're either finishing a contined parameter, or
	 * we're working on a new one.
	 */

	if (index > 0) {
	    q += snprintf(q, sizeof(line) - (q - line), "%s*%d",
	    		  params->pm_name, index);
	} else {
	    strncpy(q, params->pm_name, sizeof(line) - (q - line));
	    q += strlen(q);
	}

	if (encode)
	    i = encode_param(params, q, sizeof(line) - (q - line),
	    		     strlen(params->pm_value + valoff), valoff, index);
	else
	    i = normal_param(params, q, sizeof(line) - (q - line),
	    		     strlen(params->pm_value + valoff), valoff);

	if (i == 0) {
	    if (paramout)
		free(paramout);
	    return NULL;
	}

	paramout = add(line, paramout);
	initialwidth += strlen(line);

	params = params->pm_next;
    }

    if (offsetout)
	*offsetout = initialwidth;

    return paramout;
}

/*
 * Calculate the size of a parameter.
 *
 * Arguments include
 *
 * pm		- The parameter being output
 * index	- If continuing the parameter, the index of the section
 *		  we're on.
 * valueoff	- The current offset into the parameter value that we're
 *		  working on (previous sections have consumed valueoff bytes).
 * encode	- Set if we should perform encoding on this parameter section
 *		  (given that we're consuming bytesfit bytes).
 * cont		- Set if the remaining data in value will not fit on a single
 *		  line and will need to be continued.
 * bytesfit	- The number of bytes that we can consume from the parameter
 *		  value and still fit on a completely new line.  The
 *		  calculation assumes the new line starts with a tab,
 *		  includes the parameter name and any encoding, and fits
 *		  within CPERLIN bytes.  Will always be at least 1.
 */

static size_t
param_len(PM pm, int index, size_t valueoff, int *encode, int *cont,
	  size_t *bytesfit)
{
    char *start = pm->pm_value + valueoff, *p, indexchar[32];
    size_t len = 0, fit = 0;
    int fitlimit = 0, eightbit, maxfit;

    *encode = 0;

    /*
     * Add up the length.  First, start with the parameter name.
     */

    len = strlen(pm->pm_name);

    /*
     * Scan the parameter value and see if we need to do encoding for this
     * section.
     */

    eightbit = contains8bit(start, NULL);

    /*
     * Determine if we need to encode this section.  Encoding is necessary if:
     *
     * - There are any 8-bit characters at all and we're on the first
     *   section.
     * - There are 8-bit characters within N bytes of our section start.
     *   N is calculated based on the number of bytes it would take to
     *   reach CPERLIN.  Specifically:
     *		8 (starting tab) +
     *		strlen(param name) +
     *		4 ('* for section marker, '=', opening/closing '"')
     *		strlen (index)
     *	is the number of bytes used by everything that isn't part of the
     *  value.  So that gets subtracted from CPERLIN.
     */

    snprintf(indexchar, sizeof(indexchar), "%d", index);
    maxfit = CPERLIN - (12 + len + strlen(indexchar));
    if ((eightbit && index == 0) || contains8bit(start, start + maxfit)) {
	*encode = 1;
    }

    len++;	/* Add in equal sign */

    if (*encode) {
	/*
	 * We're using maxfit as a marker for how many characters we can
	 * fit into the line.  Bump it by two because we're not using quotes
	 * when encoding.
	 */

	maxfit += 2;

	/*
	 * If we don't have a charset or language tag in this parameter,
	 * add them now.
	 */

	if (! pm->pm_charset)
	    pm->pm_charset = getcpy(write_charset_8bit());
	if (! pm->pm_lang)
	    pm->pm_lang = getcpy(NULL);	/* Default to a blank lang tag */

	len++;		/* For the encoding marker */
	maxfit--;
	if (index == 0) {
	    int enclen = strlen(pm->pm_charset) + strlen(pm->pm_lang) + 2;
	    len += enclen;
	    maxfit-= enclen;
	} else {
	    /*
	     * We know we definitely need to include an index.  maxfit already
	     * includes the section marker.
	     */
	    len += strlen(indexchar);
	}
	for (p = start; *p != '\0'; p++) {
	    if (isparamencode(*p)) {
		len += 3;
		maxfit -= 3;
	    } else {
	    	len++;
		maxfit--;
	    }
	    /*
	     * Just so there's no confusion: maxfit is counting OUTPUT
	     * characters (post-encoding).  fit is counting INPUT characters.
	     */
	    if (! fitlimit && maxfit >= 0)
		fit++;
	    else if (! fitlimit)
		fitlimit++;
	}
    } else {
    	/*
	 * Calculate the string length, but add room for quoting \
	 * and " if necessary.  Also account for quotes at beginning
	 * and end.
	 */
	for (p = start; *p != '\0'; p++) {
	    switch (*p) {
	    case '"':
	    case '\\':
	    	len++;
		maxfit--;
	    /* FALL THROUGH */
	    default:
		len++;
		maxfit--;
	    }
	    if (! fitlimit && maxfit >= 0)
		fit++;
	    else if (! fitlimit)
		fitlimit++;
	}

	len += 2;
    }

    if (fit < 1)
	fit = 1;

    *cont = fitlimit;
    *bytesfit = fit;

    return len;
}

/*
 * Output an encoded parameter string.
 */

static size_t
encode_param(PM pm, char *output, size_t len, size_t valuelen,
	      size_t valueoff, int index)
{
    size_t outlen = 0, n;
    char *endptr = output + len, *p;

    /*
     * First, output the marker for an encoded string.
     */

    *output++ = '*';
    *output++ = '=';
    outlen += 2;

    /*
     * If the index is 0, output the character set and language tag.
     * If theses were NULL, they should have already been filled in
     * by param_len().
     */

    if (index == 0) {
	n = snprintf(output, len - outlen, "%s'%s'", pm->pm_charset,
		     pm->pm_lang);
	output += n;
	outlen += n;
	if (output > endptr) {
	    advise(NULL, "Internal error: parameter buffer overflow");
	    return 0;
	}
    }

    /*
     * Copy over the value, encoding if necessary
     */

    p = pm->pm_value + valueoff;
    while (valuelen-- > 0) {
	if (isparamencode(*p)) {
	    n = snprintf(output, len - outlen, "%%%02X", (unsigned char) *p++);
	    output += n;
	    outlen += n;
	} else {
	    *output++ = *p++;
	    outlen++;
	}
	if (output > endptr) {
	    advise(NULL, "Internal error: parameter buffer overflow");
	    return 0;
	}
    }

    *output = '\0';

    return outlen;
}

/*
 * Output a "normal" parameter, without encoding.  Be sure to escape
 * quotes and backslashes if necessary.
 */

static size_t
normal_param(PM pm, char *output, size_t len, size_t valuelen,
	     size_t valueoff)
{
    size_t outlen = 0;
    char *endptr = output + len, *p;

    *output++ = '=';
    *output++ = '"';
    outlen += 2;

    p = pm->pm_value + valueoff;

    while (valuelen-- > 0) {
	switch (*p) {
	case '\\':
	case '"':
	    *output++ = '\\';
	    outlen++;
	default:
	    *output++ = *p++;
	    outlen++;
	}
	if (output > endptr) {
	    advise(NULL, "Internal error: parameter buffer overflow");
	    return 0;
	}
    }

    if (output - 2 > endptr) {
	advise(NULL, "Internal error: parameter buffer overflow");
	return 0;
    }

    *output++ = '"';
    *output++ = '\0';

    return outlen + 1;
}

/*
 * Add a parameter to the parameter linked list
 */

PM
add_param(PM *first, PM *last, char *name, char *value, int nocopy)
{
    PM pm = mh_xmalloc(sizeof(*pm));

    memset(pm, 0, sizeof(*pm));

    pm->pm_name = nocopy ? name : getcpy(name);
    pm->pm_value = nocopy ? value : getcpy(value);

    if (*first) {
	(*last)->pm_next = pm;
	*last = pm;
    } else {
    	*first = pm;
	*last = pm;
    }

    return pm;
}

/*
 * Either replace a current parameter with a new value, or add the parameter
 * to the parameter linked list.
 */

PM
replace_param(PM *first, PM *last, char *name, char *value, int nocopy)
{
    PM pm;

    for (pm = *first; pm != NULL; pm = pm->pm_next) {
	if (strcasecmp(name, pm->pm_name) == 0) {
	    /*
	     * If nocopy is set, it's assumed that we own both name
	     * and value.  We don't need name, so we discard it now.
	     */
	    if (nocopy)
		free(name);
	    free(pm->pm_value);
	    pm->pm_value = nocopy ? value : getcpy(value);
	    return pm;
	}
    }

    return add_param(first, last, name, value, nocopy);
}

/*
 * Retrieve a parameter value from a parameter linked list.  If the parameter
 * value needs converted to the local character set, do that now.
 */

char *
get_param(PM first, const char *name, char replace, int fetchonly)
{
    while (first != NULL) {
	if (strcasecmp(name, first->pm_name) == 0) {
	    if (fetchonly)
	    	return first->pm_value;
	    else
	    	return getcpy(get_param_value(first, replace));
	}
	first = first->pm_next;
    }

    return NULL;
}

/*
 * Return a parameter value, converting to the local character set if
 * necessary
 */

char *get_param_value(PM pm, char replace)
{
    static char buffer[4096];		/* I hope no parameters are larger */
    size_t bufsize = sizeof(buffer);
#ifdef HAVE_ICONV
    size_t inbytes;
    int utf8;
    iconv_t cd;
    ICONV_CONST char *p;
#endif /* HAVE_ICONV */
    char *q;

    /*
     * If we don't have a character set indicated, it's assumed to be
     * US-ASCII.  If it matches our character set, we don't need to convert
     * anything.
     */

    if (!pm->pm_charset || check_charset(pm->pm_charset,
    					 strlen(pm->pm_charset))) {
	return pm->pm_value;
    }

    /*
     * In this case, we need to convert.  If we have iconv support, use
     * that.  Otherwise, go through and simply replace every non-ASCII
     * character with the substitution character.
     */

#ifdef HAVE_ICONV
    q = buffer;
    bufsize = sizeof(buffer);
    utf8 = strcasecmp(pm->pm_charset, "UTF-8") == 0;

    cd = iconv_open(get_charset(), pm->pm_charset);
    if (cd == (iconv_t) -1) {
	goto noiconv;
    }

    inbytes = strlen(pm->pm_value);
    p = pm->pm_value;

    while (inbytes) {
	if (iconv(cd, &p, &inbytes, &q, &bufsize) == (size_t)-1) {
	    if (errno != EILSEQ) {
		iconv_close(cd);
		goto noiconv;
	    }
	    /*
	     * Reset shift state, substitute our character,
	     * try to restart conversion.
	     */

	    iconv(cd, NULL, NULL, &q, &bufsize);

	    if (bufsize == 0) {
		iconv_close(cd);
		goto noiconv;
	    }
	    *q++ = replace;
	    bufsize--;
	    if (bufsize == 0) {
		iconv_close(cd);
		goto noiconv;
	    }
	    if (utf8) {
		for (++p, --inbytes;
		     inbytes > 0 && (((unsigned char) *q) & 0xc0) == 0x80;
		     ++p, --inbytes)
		    continue;
	    } else {
		p++;
		inbytes--;
	    }
	}
    }

    iconv_close(cd);

    if (bufsize == 0)
	q--;
    *q = '\0';

    return buffer;
#endif /* HAVE_ICONV */

noiconv:
    /*
     * Take everything non-ASCII and substituite the replacement character
     */

    q = buffer;
    bufsize = sizeof(buffer);
    for (p = pm->pm_value; *p != '\0' && bufsize > 1; p++, q++, bufsize--) {
	if (isascii((unsigned char) *p) && !iscntrl((unsigned char) *p))
	    *q = *p;
	else
	    *q = replace;
    }

    *q = '\0';

    return buffer;
}
