
/*
 * vmhsbr.h -- definitions for the vmh protocol
 *
 * $Id$
 */

#define	RC_VRSN	1

/* flags for rh_type */
#define	RC_INI	0x01		/* must be greater than OK */
#define	RC_ACK	0x02
#define	RC_ERR	0x03
#define	RC_CMD	0x04
#define	RC_QRY	0x05
#define	RC_TTY	0x06
#define	RC_WIN	0x07
#define	RC_DATA	0x08
#define	RC_EOF	0x09
#define	RC_FIN	0x0a
#define	RC_XXX	0x0b

struct record {
    struct rcheader {
	char rh_type;		/* type of record   */
	int  rh_len;		/* length of data   */
    } rc_header;
    char *rc_data;		/* extensible array */
};

#define	rc_head(rc)	(&rc->rc_header)
#define	RHSIZE(rc)	(sizeof rc->rc_header)
#define	rc_type		rc_header.rh_type
#define	rc_len		rc_header.rh_len

#define	initrc(rc) rc->rc_data = NULL

/*
 * prototypes
 */
int rcinit (int, int);
int rcdone (void);
int rc2rc (char, int, char *, struct record *);
int str2rc (char, char *, struct record *);
int peer2rc (struct record *);
int rc2peer (char, int, char *);
int str2peer (char, char *);
int fmt2peer (char, char *, ...);
int err2peer (char, char *, char *, ...);
int verr2peer (char, char *, char *, va_list);

