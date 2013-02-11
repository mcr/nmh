
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
typedef struct cefile  *CE;
typedef struct CTinfo  *CI;
typedef struct Content *CT;

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
 * Structure for storing parsed elements
 * of the Content-Type component.
 */
struct CTinfo {
    char *ci_type;		/* content type     */
    char *ci_subtype;		/* content subtype  */
    char *ci_attrs[NPARMS + 2];	/* attribute names  */
    char *ci_values[NPARMS];	/* attribute values */
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
    char *c_partno;		/* within multipart content          */

    /* Content-Type info */
    struct CTinfo c_ctinfo;	/* parsed elements of Content-Type   */
    int	c_type;			/* internal flag for content type    */
    int	c_subtype;		/* internal flag for content subtype */

    /* Content-Transfer-Encoding info (decoded contents) */
    CE c_cefile;		/* structure holding decoded content */
    int	c_encoding;		/* internal flag for encoding type   */

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
    pid_t c_pid;		/* process doing display             */
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
int pidcheck (int);
CT parse_mime (char *);
int add_header (CT, char *, char *);
int get_ctinfo (char *, CT, int);
int params_external (CT, int);
int open7Bit (CT, char **);
void close_encoding (CT);
void free_content (CT);

extern int checksw;	/* Add Content-MD5 field */
