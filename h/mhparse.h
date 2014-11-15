
/*
 * mhparse.h -- definitions for parsing/building of MIME content
 *           -- (mhparse.c/mhbuildsbr.c)
 */

#define	NPARTS	50
#define	NTYPES	20
#define	NPARMS	10

/*
 * Abstract type for header fields
 */
typedef struct hfield *HF;

/*
 * Abstract types for MIME parsing/building
 */
typedef struct cefile		*CE;
typedef struct CTinfo		*CI;
typedef struct Content		*CT;
typedef struct Parameter	*PM;

/*
 * type for Init function (both type and transfer encoding)
 */
typedef int (*InitFunc) (CT);

/*
 * types for various transfer encoding access functions
 */
typedef int (*OpenCEFunc) (CT, char **);
typedef void (*CloseCEFunc) (CT);
typedef unsigned long (*SizeCEFunc) (CT);

/*
 * Structure for storing/encoding/decoding
 * a header field and its value.
 */
struct hfield {
    char *name;		/* field name */
    char *value;	/* field body */
    int hf_encoding;	/* internal flag for transfer encoding to use */
    HF next;		/* link to next header field */
};

/*
 * Structure for holding MIME parameter elements.
 */
struct Parameter {
    char *pm_name;	/* Parameter name */
    char *pm_value;	/* Parameter value */
    char *pm_charset;	/* Parameter character set (optional) */
    char *pm_lang;	/* Parameter language tag (optional) */
    PM   pm_next;	/* Pointer to next element */
};

/*
 * Structure for storing parsed elements
 * of the Content-Type component.
 */
struct CTinfo {
    char *ci_type;		/* content type     */
    char *ci_subtype;		/* content subtype  */
    PM   ci_first_pm;		/* Pointer to first MIME parameter */
    PM   ci_last_pm;		/* Pointer to last MIME parameter */
    char *ci_comment;		/* RFC-822 comments */
    char *ci_magic;
};

/*
 * Structure for storing decoded contents after
 * removing Content-Transfer-Encoding.
 */
struct cefile {
    char *ce_file;	/* decoded content (file)   */
    FILE *ce_fp;	/* decoded content (stream) */
    int	  ce_unlink;	/* remove file when done?   */
};

/*
 * Primary structure for handling Content (Entity)
 *
 * Some more explanation of this:
 *
 * This structure recursively describes a complete MIME message.
 * At the top level, the c_first_hf list has a list of all message
 * headers.  If the content-type is multipart (c_type == CT_MULTIPART)
 * then c_ctparams will contain a pointer to a struct multipart.
 * A struct multipart contains (among other trhings) a linked list
 * of struct part elements, and THOSE contain a pointer to the sub-part's
 * Content structure.
 */
struct Content {
    /* source (read) file */
    char *c_file;		/* read contents (file)              */
    FILE *c_fp;			/* read contents (stream)            */
    int	c_unlink;		/* remove file when done?            */

    long c_begin;		/* where content body starts in file */
    long c_end;			/* where content body ends in file   */

    /* linked list of header fields */
    HF c_first_hf;		/* pointer to first header field     */
    HF c_last_hf;		/* pointer to last header field      */

    /* copies of MIME related header fields */
    char *c_vrsn;		/* MIME-Version:                     */
    char *c_ctline;		/* Content-Type:                     */
    char *c_celine;		/* Content-Transfer-Encoding:        */
    char *c_id;			/* Content-ID:                       */
    char *c_descr;		/* Content-Description:              */
    char *c_dispo;		/* Content-Disposition:              */
    char *c_dispo_type;		/* Type of Content-Disposition       */
    PM c_dispo_first;		/* Pointer to first disposition parm */
    PM c_dispo_last;		/* Pointer to last disposition parm  */
    char *c_partno;		/* within multipart content          */

    /* Content-Type info */
    struct CTinfo c_ctinfo;	/* parsed elements of Content-Type   */
    int	c_type;			/* internal flag for content type    */
    int	c_subtype;		/* internal flag for content subtype */

    /* Content-Transfer-Encoding info (decoded contents) */
    struct cefile c_cefile;	/* structure holding decoded content */
    int	c_encoding;		/* internal flag for encoding type   */
    int c_reqencoding;		/* Requested encoding (by mhbuild)   */

    /* Content-MD5 info */
    int	c_digested;		/* have we seen this header before?  */
    unsigned char c_digest[16];	/* decoded MD5 checksum              */

    /* pointers to content-specific structures */
    void *c_ctparams;		/* content type specific data        */
    struct exbody *c_ctexbody;	/* data for type message/external    */

    /* function pointers */
    InitFunc    c_ctinitfnx;	/* parse content body                */
    OpenCEFunc  c_ceopenfnx;	/* get a stream to decoded contents  */
    CloseCEFunc c_ceclosefnx;	/* release stream                    */
    SizeCEFunc  c_cesizefnx;	/* size of decoded contents          */

    int	c_umask;		/* associated umask                  */
    int	c_rfc934;		/* rfc934 compatibility flag         */

    char *c_showproc;		/* default, if not in profile        */
    char *c_termproc;		/* for charset madness...            */
    char *c_storeproc;		/* overrides profile entry, if any   */

    char *c_storage;		/* write contents (file)             */
    char *c_folder;		/* write contents (folder)           */
};

/*
 * Flags for Content-Type (Content->c_type)
 */
#define	CT_UNKNOWN	0x00
#define	CT_APPLICATION	0x01
#define	CT_AUDIO	0x02
#define	CT_IMAGE	0x03
#define	CT_MESSAGE	0x04
#define	CT_MULTIPART	0x05
#define	CT_TEXT		0x06
#define	CT_VIDEO	0x07
#define	CT_EXTENSION	0x08

/*
 * Flags for Content-Transfer-Encoding (Content->c_encoding)
 */
#define	CE_UNKNOWN	0x00
#define	CE_BASE64	0x01
#define	CE_QUOTED	0x02
#define	CE_8BIT		0x03
#define	CE_7BIT		0x04
#define	CE_BINARY	0x05
#define	CE_EXTENSION	0x06
#define	CE_EXTERNAL	0x07	/* for external-body */

/*
 * TEXT content
 */

/* Flags for subtypes of TEXT */
#define	TEXT_UNKNOWN	0x00
#define	TEXT_PLAIN	0x01
#define	TEXT_RICHTEXT	0x02
#define TEXT_ENRICHED	0x03

/* Flags for character sets */
#define	CHARSET_SPECIFIED    0x00
#define CHARSET_UNSPECIFIED 0x01  /* only needed when building drafts */

/* Structure for text content */
struct text {
    int	tx_charset;		/* flag for character set */
};

/*
 * MULTIPART content
 */

/* Flags for subtypes of MULTIPART */
#define	MULTI_UNKNOWN	0x00
#define	MULTI_MIXED	0x01
#define	MULTI_ALTERNATE	0x02
#define	MULTI_DIGEST	0x03
#define	MULTI_PARALLEL	0x04

/* Structure for subparts of a multipart content */
struct part {
    CT mp_part;			/* Content structure for subpart     */
    struct part *mp_next;	/* pointer to next subpart structure */
};

/* Main structure for multipart content */
struct multipart {
    char *mp_start;		/* boundary string separating parts   */
    char *mp_stop;		/* terminating boundary string        */
    char *mp_content_before;	/* any content before the first subpart */
    char *mp_content_after;	/* any content after the last subpart */
    struct part *mp_parts;	/* pointer to first subpart structure */
};

/*
 * MESSAGE content
 */

/* Flags for subtypes of MESSAGE */
#define	MESSAGE_UNKNOWN	 0x00
#define	MESSAGE_RFC822	 0x01
#define	MESSAGE_PARTIAL	 0x02
#define	MESSAGE_EXTERNAL 0x03

/* Structure for message/partial */
struct partial {
    char *pm_partid;
    int	pm_partno;
    int	pm_maxno;
    int	pm_marked;
    int	pm_stored;
};

/* Structure for message/external */
struct exbody {
    CT eb_parent;	/* pointer to controlling content structure */
    CT eb_content;	/* pointer to internal content structure    */
    char *eb_partno;
    char *eb_access;
    int	eb_flags;
    char *eb_name;
    char *eb_permission;
    char *eb_site;
    char *eb_dir;
    char *eb_mode;
    unsigned long eb_size;
    char *eb_server;
    char *eb_subject;
    char *eb_body;
    char *eb_url;
};

/*
 * APPLICATION content
 */

/* Flags for subtype of APPLICATION */
#define	APPLICATION_UNKNOWN	0x00
#define	APPLICATION_OCTETS	0x01
#define	APPLICATION_POSTSCRIPT	0x02


/*
 * Structures for mapping types to their internal flags
 */
struct k2v {
    char *kv_key;
    int	  kv_value;
};
extern struct k2v SubText[];
extern struct k2v Charset[];
extern struct k2v SubMultiPart[];
extern struct k2v SubMessage[];
extern struct k2v SubApplication[];

/*
 * Structures for mapping (content) types to
 * the functions to handle them.
 */
struct str2init {
    char *si_key;
    int	  si_val;
    InitFunc si_init;
};
extern struct str2init str2cts[];
extern struct str2init str2ces[];
extern struct str2init str2methods[];

/*
 * prototypes
 */
CT parse_mime (char *);

/*
 * Translate a composition file into a MIME data structure.  Arguments are:
 *
 * infile	- Name of input filename
 * autobuild    - A flag to indicate if the composition file parser is
 *		  being run in automatic mode or not.  In auto mode,
 *		  if a MIME-Version header is encountered it is assumed
 *		  that the composition file is already in MIME format
 *		  and will not be processed further.  Otherwise, an
 *		  error is generated.
 * dist		- A flag to indicate if we are being run by "dist".  In
 *		  that case, add no MIME headers to the message.  Existing
 *		  headers will still be encoded by RFC 2047.
 * directives	- A flag to control whether or not build directives are
 *		  processed by default.
 * encoding	- The default encoding to use when doing RFC 2047 header
 *		  encoding.  Must be one of CE_UNKNOWN, CE_BASE64, or
 *		  CE_QUOTED.
 * maxunencoded	- The maximum line length before the default encoding for
 *		  text parts is quoted-printable.
 * verbose	- If 1, output verbose information during message composition
 *
 * Returns a CT structure describing the resulting MIME message.  If the
 * -auto flag is set and a MIME-Version header is encountered, the return
 * value is NULL.
 */
CT build_mime (char *infile, int autobuild, int dist, int directives,
	       int encoding, size_t maxunencoded, int verbose);

int add_header (CT, char *, char *);
int get_ctinfo (char *, CT, int);
int params_external (CT, int);
int open7Bit (CT, char **);
void close_encoding (CT);
void free_content (CT);
char *ct_type_str (int);
char *ct_subtype_str (int, int);
const struct str2init *get_ct_init (int);
const char *ce_str (int);
const struct str2init *get_ce_method (const char *);
char *content_charset (CT);
int convert_charset (CT, char *, int *);
void reverse_alternative_parts (CT);

/*
 * Given a content structure, return true if the content has a disposition
 * of "inline".
 *
 * Arguments are:
 *
 * ct		- Content structure to examine
 */
int is_inline(CT ct);

/*
 * Given a list of messages, display information about them on standard
 * output.
 *
 * Arguments are:
 *
 * cts		- An array of CT elements of messages that need to be
 *		  displayed.  Array is terminated by a NULL.
 * headsw	- If 1, display a column header.
 * sizesw	- If 1, display the size of the part.
 * verbosw	- If 1, display verbose information
 * debugsw	- If 1, turn on debugging for the output.
 * disposw	- If 1, display MIME part disposition information.
 *
 */
void list_all_messages(CT *cts, int headsw, int sizesw, int verbosw,
		       int debugsw, int disposw);

/*
 * List the content information of a single MIME part on stdout.
 *
 * Arguments are:
 *
 * ct		- MIME Content structure to display.
 * toplevel	- If set, we're at the top level of a message
 * realsize	- If set, determine the real size of the content
 * verbose	- If set, output verbose information
 * debug	- If set, turn on debugging for the output
 * dispo	- If set, display MIME part disposition information.
 *
 * Returns OK on success, NOTOK otherwise.
 */
int list_content(CT ct, int toplevel, int realsize, int verbose, int debug,
		 int dispo);

/*
 * Display content-appropriate information on MIME parts, decending recursively
 * into multipart content if appropriate.  Uses list_content() for displaying
 * generic information.
 *
 * Arguments and return value are the same as list_content().
 */
int list_switch(CT ct, int toplevel, int realsize, int verbose, int debug,
		int dispo);

/*
 * Given a linked list of parameters, build an output string for them.  This
 * string is designed to be concatenated on an already-built header.
 *
 * Arguments are:
 *
 * initialwidth	- Current width of the header.  Used to compute when to wrap
 *		  parameters on the first line.  The following lines will
 *		  be prefixed by a tab (\t) character.
 * params	- Pointer to head of linked list of parameters.
 * offsetout	- The final line offset after all the parameters have been
 *		  output.  May be NULL.
 * external	- If set, outputting an external-body type and will not
 *		  output a "body" parameter.
 
 * Returns a pointer to the resulting parameter string.  This string must
 * be free()'d by the caller.  Returns NULL on error.
 */
char *output_params(size_t initialwidth, PM params, int *offsetout,
		    int external);

/*
 * Add a parameter to the parameter linked list.
 *
 * Arguments are:
 *
 * first	- Pointer to head of linked list
 * last		- Pointer to tail of linked list
 * name		- Name of parameter
 * value	- Value of parameter
 * nocopy	- If set, will use the pointer values directly for "name"
 *		  and "value" instead of making their own copy.  These
 *		  pointers will be free()'d later by the MIME routines, so
 *		  they should not be used after calling this function!
 *
 * Returns allocated parameter element
 */
PM add_param(PM *first, PM *last, char *name, char *value, int nocopy);

/*
 * Replace (or add) a parameter to the parameter linked list.
 *
 * If the named parameter already exists on the parameter linked list,
 * replace the value with the new one.  Otherwise add it to the linked
 * list.  All parameters are identical to add_param().
 */
PM replace_param(PM *first, PM *last, char *name, char *value, int nocopy);

/*
 * Retrieve a parameter value from a parameter linked list.  Convert to the
 * local character set if required.
 *
 * Arguments are:
 *
 * first	- Pointer to head of parameter linked list.
 * name		- Name of parameter.
 * replace	- If characters in the parameter list cannot be converted to
 *		  the local character set, replace with this character.
 * fetchonly	- If true, return pointer to original value, no conversion
 *		  performed.
 *
 * Returns parameter value if found, NULL otherwise.  Memory must be free()'d
 * unless fetchonly is set.
 */

char *get_param(PM first, const char *name, char replace, int fetchonly);

/*
 * Fetch a parameter value from a parameter structure, converting it to
 * the local character set.
 *
 * Arguments are:
 *
 * pm		- Pointer to parameter structure
 * replace	- If characters in the parameter list cannot be converted to
 *		  the local character set, replace with this character.
 *
 * Returns a pointer to the parameter value.  Memory is stored in an
 * internal buffer, so the returned value is only valid until the next
 * call to get_param_value() or get_param() (get_param() uses get_param_value()
 * internally).
 */
char *get_param_value(PM pm, char replace);

/*
 * Display MIME message(s) on standard out.
 *
 * Arguments are:
 *
 * cts		- NULL terminated array of CT structures for messages
 *		  to display
 * concat	- If true, concatenate all MIME parts.  If false, show each
 *		  MIME part under a separate pager.
 * textonly	- If true, only display "text" MIME parts
 * inlineonly	- If true, only display MIME parts that are marked with
 *		  a disposition of "inline" (includes parts that lack a
 *		  Content-Disposition header).
 * markerform	- The name of a file containg mh-format(5) code used to
 *		  display markers about non-displayed MIME parts.
 */
void show_all_messages(CT *cts, int concat, int textonly, int inlineonly,
		       char *markerform);

/*
 * Display (or store) a single MIME part using the specified command
 *
 * Arguments are:
 *
 * ct		- The Content structure of the MIME part we wish to display
 * alternate	- Set this to true if this is one part of a MIME
 *		  multipart/alternative part.  Will suppress some errors and
 *		  will cause the function to return DONE instead of OK on
 *		  success.
 * cp		- The command string to execute.  Will be run through the
 *		  parser for %-escapes as described in mhshow(1).
 * cracked	- If set, chdir() to this directory before executing the
 *		  command in "cp".  Only used by mhstore(1).
 * fmt		- A series of mh-format(5) instructions to execute if the
 *		  command string indicates a marker is desired.  Can be NULL.
 *
 * Returns NOTOK if we could not display the part, DONE if alternate was
 * set and we could display the part, and OK if alternate was not set and
 * we could display the part.
 */
struct format;
int show_content_aux(CT ct, int alternate, char *cp, char *cracked,
		     struct format *fmt);

extern int checksw;	/* Add Content-MD5 field */

/*
 * mhstore
 * Put it here because it uses the CT typedef.
 */
typedef struct mhstoreinfo *mhstoreinfo_t;
mhstoreinfo_t mhstoreinfo_create(CT *, char *, const char *, int, int);
int mhstoreinfo_files_not_clobbered(const mhstoreinfo_t);
void mhstoreinfo_free(mhstoreinfo_t);
void store_all_messages (mhstoreinfo_t);
