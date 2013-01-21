
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
#include <errno.h>
#include <signal.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/utils.h>


extern int debugsw;

extern pid_t xpid;	/* mhshowsbr.c  */

/* cache policies */
extern int rcachesw;	/* mhcachesbr.c */
extern int wcachesw;	/* mhcachesbr.c */

int checksw = 0;	/* check Content-MD5 field */

/*
 * Directory to place temp files.  This must
 * be set before these routines are called.
 */
char *tmp;

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


/* mhcachesbr.c */
int find_cache (CT, int, int *, char *, char *, int);

/* mhmisc.c */
int part_ok (CT, int);
int type_ok (CT, int);
void content_error (char *, CT, char *, ...);

/* mhfree.c */
void free_content (CT);
void free_encoding (CT, int);

/*
 * static prototypes
 */
static CT get_content (FILE *, char *, int);
static int get_comment (CT, unsigned char **, int);

static int InitGeneric (CT);
static int InitText (CT);
static int InitMultiPart (CT);
static void reverse_parts (CT);
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
            advise("mhparse", "unable to create temporary file");
            return NULL;
        }
	file = add (tfile, NULL);
	chmod (file, 0600);

	while (fgets (buffer, sizeof(buffer), stdin))
	    fputs (buffer, fp);
	fflush (fp);

	if (ferror (stdin)) {
	    unlink (file);
	    advise ("stdin", "error reading");
	    return NULL;
	}
	if (ferror (fp)) {
	    unlink (file);
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
	    unlink (file);
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
    m_getfld_state_t gstate;

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
    m_getfld_state_init (&gstate);
    for (compnum = 1;;) {
	int bufsz = sizeof buf;
	switch (state = m_getfld (gstate, name, buf, &bufsz, in)) {
	case FLD:
	case FLDPLUS:
	    compnum++;

	    /* get copies of the buffers */
	    np = add (name, NULL);
	    vp = add (buf, NULL);

	    /* if necessary, get rest of field */
	    while (state == FLDPLUS) {
		bufsz = sizeof buf;
		state = m_getfld (gstate, name, buf, &bufsz, in);
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
	if (!mh_strcasecmp (hp->name, VRSN_FIELD)) {
	    int ucmp;
	    char c;
	    unsigned char *cp, *dp;

	    if (ct->c_vrsn) {
		advise (NULL, "message %s has multiple %s: fields",
			ct->c_file, VRSN_FIELD);
		goto next_header;
	    }
	    ct->c_vrsn = add (hp->value, NULL);

	    /* Now, cleanup this field */
	    cp = ct->c_vrsn;

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
	    c = *dp;
	    *dp = '\0';
	    ucmp = !mh_strcasecmp (cp, VRSN_VALUE);
	    *dp = c;
	    if (!ucmp) {
		admonish (NULL, "message %s has unknown value for %s: field (%s)",
		ct->c_file, VRSN_FIELD, cp);
	    }
	}
	else if (!mh_strcasecmp (hp->name, TYPE_FIELD)) {
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
		if (!mh_strcasecmp (ci->ci_type, s2i->si_key))
		    break;
	    if (!s2i->si_key && !uprf (ci->ci_type, "X-"))
		s2i++;
	    ct->c_type = s2i->si_val;
	    ct->c_ctinitfnx = s2i->si_init;
	}
	else if (!mh_strcasecmp (hp->name, ENCODING_FIELD)) {
	/* Get Content-Transfer-Encoding field */
	    char c;
	    unsigned char *cp, *dp;
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
		if (!mh_strcasecmp (cp, s2i->si_key))
		    break;
	    if (!s2i->si_key && !uprf (cp, "X-"))
		s2i++;
	    *dp = c;
	    ct->c_encoding = s2i->si_val;

	    /* Call the Init function for this encoding */
	    if (s2i->si_init && (*s2i->si_init) (ct) == NOTOK)
		goto out;
	}
	else if (!mh_strcasecmp (hp->name, MD5_FIELD)) {
	/* Get Content-MD5 field */
	    unsigned char *cp, *dp;
	    char *ep;

	    if (!checksw)
		goto next_header;

	    if (ct->c_digested) {
		advise (NULL, "message %s has multiple %s: fields",
			ct->c_file, MD5_FIELD);
		goto next_header;
	    }

	    ep = cp = add (hp->value, NULL);	/* get a copy */

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
	}
	else if (!mh_strcasecmp (hp->name, ID_FIELD)) {
	/* Get Content-ID field */
	    ct->c_id = add (hp->value, ct->c_id);
	}
	else if (!mh_strcasecmp (hp->name, DESCR_FIELD)) {
	/* Get Content-Description field */
	    ct->c_descr = add (hp->value, ct->c_descr);
	}
	else if (!mh_strcasecmp (hp->name, DISPO_FIELD)) {
	/* Get Content-Disposition field */
	    ct->c_dispo = add (hp->value, ct->c_dispo);
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


/* Make sure that buf contains at least one appearance of name,
   followed by =.  If not, insert both name and value, just after
   first semicolon, if any.  Note that name should not contain a
   trailing =.	And quotes will be added around the value.  Typical
   usage:  make sure that a Content-Disposition header contains
   filename="foo".  If it doesn't and value does, use value from
   that. */
static char *
incl_name_value (unsigned char *buf, char *name, char *value) {
    char *newbuf = buf;

    /* Assume that name is non-null. */
    if (buf && value) {
	char *name_plus_equal = concat (name, "=", NULL);

	if (! strstr (buf, name_plus_equal)) {
	    char *insertion;
	    unsigned char *cp;
	    char *prefix, *suffix;

	    /* Trim trailing space, esp. newline. */
	    for (cp = &buf[strlen (buf) - 1];
		 cp >= buf && isspace (*cp);
		 --cp) {
		*cp = '\0';
	    }

	    insertion = concat ("; ", name, "=", "\"", value, "\"", NULL);

	    /* Insert at first semicolon, if any.  If none, append to
	       end. */
	    prefix = add (buf, NULL);
	    if ((cp = strchr (prefix, ';'))) {
		suffix = concat (cp, NULL);
		*cp = '\0';
		newbuf = concat (prefix, insertion, suffix, "\n", NULL);
		free (suffix);
	    } else {
		/* Append to end. */
		newbuf = concat (buf, insertion, "\n", NULL);
	    }

	    free (prefix);
	    free (insertion);
	    free (buf);
	}

	free (name_plus_equal);
    }

    return newbuf;
}

/* Extract just name_suffix="foo", if any, from value.	If there isn't
   one, return the entire value.  Note that, for example, a name_suffix
   of name will match filename="foo", and return foo. */
static char *
extract_name_value (char *name_suffix, char *value) {
    char *extracted_name_value = value;
    char *name_suffix_plus_quote = concat (name_suffix, "=\"", NULL);
    char *name_suffix_equals = strstr (value, name_suffix_plus_quote);
    char *cp;

    free (name_suffix_plus_quote);
    if (name_suffix_equals) {
	char *name_suffix_begin;

	/* Find first \". */
	for (cp = name_suffix_equals; *cp != '"'; ++cp) /* empty */;
	name_suffix_begin = ++cp;
	/* Find second \". */
	for (; *cp != '"'; ++cp) /* empty */;

	extracted_name_value = mh_xmalloc (cp - name_suffix_begin + 1);
	memcpy (extracted_name_value,
		name_suffix_begin,
		cp - name_suffix_begin);
	extracted_name_value[cp - name_suffix_begin] = '\0';
    }

    return extracted_name_value;
}

/*
 * Parse Content-Type line and (if `magic' is non-zero) mhbuild composition
 * directives.  Fills in the information of the CTinfo structure.
 */
int
get_ctinfo (unsigned char *cp, CT ct, int magic)
{
    int	i;
    unsigned char *dp;
    char **ap, **ep;
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
	char *vp;
	unsigned char *up;

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

	for (up = dp; isspace (*dp);)
	    dp++;
	if (dp == cp || *dp != '=') {
	    advise (NULL,
		    "invalid parameter in message %s's %s: field\n%*.*sparameter %s (error detected at offset %d)",
		    ct->c_file, TYPE_FIELD, i, i, "", cp, dp - cp);
	    return NOTOK;
	}

	vp = (*ap = add (cp, NULL)) + (up - cp);
	*vp = '\0';
	for (dp++; isspace (*dp);)
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
     * Get any {Content-Disposition} given in buffer.
     */
    if (magic && *cp == '{') {
        ct->c_dispo = ++cp;
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
	if (*ct->c_dispo)
	    ct->c_dispo = concat (ct->c_dispo, "\n", NULL);
	else
	    ct->c_dispo = NULL;
	*dp++ = c;
	cp = dp;

	while (isspace (*cp))
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
            if (ct->c_dispo)
                ct->c_dispo =
                    incl_name_value (ct->c_dispo,
                                     "filename",
                                     r1bindex (extract_name_value ("name",
                                                                   ci->
                                                                   ci_magic),
                                               '/'));
        }
	else
	    advise (NULL,
		    "extraneous information in message %s's %s: field\n%*.*s(%s)",
                    ct->c_file, TYPE_FIELD, i, i, "", cp);
    }

    return OK;
}


static int
get_comment (CT ct, unsigned char **ap, int istype)
{
    int i;
    char *bp;
    unsigned char *cp;
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
    char **ap, **ep, *cp;
    struct k2v *kv;
    struct text *t;
    CI ci = &ct->c_ctinfo;

    /* check for missing subtype */
    if (!*ci->ci_subtype)
	ci->ci_subtype = add ("plain", ci->ci_subtype);

    /* match subtype */
    for (kv = SubText; kv->kv_key; kv++)
	if (!mh_strcasecmp (ci->ci_subtype, kv->kv_key))
	    break;
    ct->c_subtype = kv->kv_value;

    /* allocate text character set structure */
    if ((t = (struct text *) calloc (1, sizeof(*t))) == NULL)
	adios (NULL, "out of memory");
    ct->c_ctparams = (void *) t;

    /* scan for charset parameter */
    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++)
	if (!mh_strcasecmp (*ap, "charset"))
	    break;

    /* check if content specified a character set */
    if (*ap) {
	chset = *ep;
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
    unsigned char *cp, *dp;
    char **ap, **ep;
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
	if (!mh_strcasecmp (ci->ci_subtype, kv->kv_key))
	    break;
    ct->c_subtype = kv->kv_value;

    /*
     * Check for "boundary" parameter, which is
     * required for multipart messages.
     */
    bp = 0;
    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
	if (!mh_strcasecmp (*ap, "boundary")) {
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
	if (!mh_strcasecmp (ci->ci_subtype, kv->kv_key))
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
		    if (!mh_strcasecmp (*ap, "id")) {
			p->pm_partid = add (*ep, NULL);
			continue;
		    }
		    if (!mh_strcasecmp (*ap, "number")) {
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
		    if (!mh_strcasecmp (*ap, "total")) {
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
    char **ap, **ep;
    struct exbody *e = (struct exbody *) ct->c_ctparams;
    CI ci = &ct->c_ctinfo;

    for (ap = ci->ci_attrs, ep = ci->ci_values; *ap; ap++, ep++) {
	if (!mh_strcasecmp (*ap, "access-type")) {
	    struct str2init *s2i;
	    CT p = e->eb_content;

	    for (s2i = str2methods; s2i->si_key; s2i++)
		if (!mh_strcasecmp (*ep, s2i->si_key))
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
	if (!mh_strcasecmp (*ap, "name")) {
	    e->eb_name = *ep;
	    continue;
	}
	if (!mh_strcasecmp (*ap, "permission")) {
	    e->eb_permission = *ep;
	    continue;
	}
	if (!mh_strcasecmp (*ap, "site")) {
	    e->eb_site = *ep;
	    continue;
	}
	if (!mh_strcasecmp (*ap, "directory")) {
	    e->eb_dir = *ep;
	    continue;
	}
	if (!mh_strcasecmp (*ap, "mode")) {
	    e->eb_mode = *ep;
	    continue;
	}
	if (!mh_strcasecmp (*ap, "size")) {
	    sscanf (*ep, "%lu", &e->eb_size);
	    continue;
	}
	if (!mh_strcasecmp (*ap, "server")) {
	    e->eb_server = *ep;
	    continue;
	}
	if (!mh_strcasecmp (*ap, "subject")) {
	    e->eb_subject = *ep;
	    continue;
	}
	if (composing && !mh_strcasecmp (*ap, "body")) {
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
	if (!mh_strcasecmp (ci->ci_subtype, kv->kv_key))
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
    CE ce;

    if ((ce = (CE) calloc (1, sizeof(*ce))) == NULL)
	adios (NULL, "out of memory");

    ct->c_cefile     = ce;
    ct->c_ceopenfnx  = openfnx;
    ct->c_ceclosefnx = close_encoding;
    ct->c_cesizefnx  = size_encoding;

    return OK;
}


void
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
    int fd, len, skip, own_ct_fp = 0;
    uint32_t bits;
    unsigned char value, b;
    unsigned char *cp, *ep;
    char buffer[BUFSIZ];
    /* sbeck -- handle suffixes */
    CI ci;
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
	ce->ce_file = add (m_mktemp(tmp, NULL, NULL), NULL);
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
            /* Temporary file already exists, so we rename to
               version with extension. */
            char *file_org = strdup(ce->ce_file);
            ce->ce_file = add (cp, ce->ce_file);
            if (rename(file_org, ce->ce_file)) {
                adios (ce->ce_file, "unable to rename %s to ", file_org);
            }
            free(file_org);

        } else {
            ce->ce_file = add (cp, ce->ce_file);
        }
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
		    	b = (bits >> 16) & 0xff;
			putc ((char) b, ce->ce_fp);
			if (digested)
			    MD5Update (&mdContext, &b, 1);
			if (skip < 2) {
			    b = (bits >> 8) & 0xff;
			    putc ((char) b, ce->ce_fp);
			    if (digested)
				MD5Update (&mdContext, &b, 1);
			    if (skip < 1) {
			    	b = bits & 0xff;
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
    unsigned char *cp, *ep;
    char buffer[BUFSIZ];
    unsigned char mask;
    CE ce;
    /* sbeck -- handle suffixes */
    CI ci;
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
	ce->ce_file = add (m_mktemp(tmp, NULL, NULL), NULL);
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
            /* Temporary file already exists, so we rename to
               version with extension. */
            char *file_org = strdup(ce->ce_file);
            ce->ce_file = add (cp, ce->ce_file);
            if (rename(file_org, ce->ce_file)) {
                adios (ce->ce_file, "unable to rename %s to ", file_org);
            }
            free(file_org);

        } else {
            ce->ce_file = add (cp, ce->ce_file);
        }
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
	    if (!isspace (*ep))
		break;
	*++ep = '\n', ep++;

	for (; cp < ep; cp++) {
	    if (quoted > 0) {
		/* in an escape sequence */
		if (quoted == 1) {
		    /* at byte 1 of an escape sequence */
		    mask = hex2nib[*cp & 0x7f];
		    /* next is byte 2 */
		    quoted = 2;
		} else {
		    /* at byte 2 of an escape sequence */
		    mask <<= 4;
		    mask |= hex2nib[*cp & 0x7f];
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
		} else if (isxdigit (cp[1]) && isxdigit (cp[2])) {
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
	ce->ce_file = add (m_mktemp(tmp, NULL, NULL), NULL);
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
            /* Temporary file already exists, so we rename to
               version with extension. */
            char *file_org = strdup(ce->ce_file);
            ce->ce_file = add (cp, ce->ce_file);
            if (rename(file_org, ce->ce_file)) {
                adios (ce->ce_file, "unable to rename %s to ", file_org);
            }
            free(file_org);

        } else {
            ce->ce_file = add (cp, ce->ce_file);
        }
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

    if ((!e->eb_permission || mh_strcasecmp (e->eb_permission, "read-write"))
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
    if ((!e->eb_permission || mh_strcasecmp (e->eb_permission, "read-write"))
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
	ce->ce_file = add (m_mktemp(tmp, NULL, NULL), NULL);

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
	vec[vecp++] = e->eb_mode && !mh_strcasecmp (e->eb_mode, "ascii")
	    		? "ascii" : "binary";
	vec[vecp] = NULL;

	fflush (stdout);

	for (i = 0; (child_id = vfork()) == NOTOK && i < 5; i++)
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

    for (i = 0; (child_id = vfork()) == NOTOK && i < 5; i++)
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
	ce->ce_file = add (m_mktemp(tmp, NULL, NULL), NULL);
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
