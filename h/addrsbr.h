
/*
 * addrsbr.h -- definitions for the address parsing system
 */

#define	AD_HOST	1		/* getm(): lookup official hostname    */
#define	AD_NHST	0		/* getm(): do not lookup official name */
#define	AD_NAME	AD_NHST		/* AD_HOST is TOO slow                 */

#define	UUCPHOST	(-1)
#define	LOCALHOST	0
#define	NETHOST		1
#define	BADHOST		2

/*
 * The email structure used by nmh to define an email address
 */

struct mailname {
    struct mailname *m_next;	/* Linked list linkage; available for */
				/* application use */
    char *m_text;		/* Full unparsed text of email address */
    char *m_pers;		/* display-name in RFC 5322 parlance */
    char *m_mbox;		/* local-part in RFC 5322 parlance */
    char *m_host;		/* domain in RFC 5322 parlance */
    char *m_path;		/* Host routing; should not be used */
    int m_type;			/* UUCPHOST, LOCALHOST, NETHOST, or BADHOST */
    char m_nohost;		/* True if no host part available */
    char m_bcc;			/* Used by post to keep track of bcc's */
    int m_ingrp;		/* True if email address is in a group */
    char *m_gname;		/* display-name of group */
    char *m_note;		/* Note (post-address comment) */
};

#define	adrformat(m) auxformat ((m), 1)

/*
 *  prototypes
 */
void mnfree(struct mailname *);
int ismymbox(struct mailname *);

/*
 * Parse an address header, and return a sequence of email addresses.
 * This function is the main entry point into the nmh address parser.
 * It is used in conjunction with getm() to parse an email header.
 *
 * Arguments include:
 *
 * header	- Pointer to the start of an email header.
 *
 * On the first call, header is copied and saved internally.  Each email
 * address in the header is returned on the first and subsequent calls
 * to getname().  When there are no more email addresses available in
 * the header, NULL is returned and the parser's internal state is
 * reset.
 */

char *getname(const char *header);
char *getlocaladdr(void);
char *auxformat(struct mailname *, int);

/*
 * Parse an email address into it's components.
 *
 * Used in conjunction with getname() to parse a complete email header.
 *
 * Arguments include:
 *
 * str		- Email address being parsed.
 * dfhost	- A default host to append to the email address if
 *		  one is not included.  If NULL, use nmh's idea of
 *		  localhost().
 * dftype	- If dfhost is given, use dftype as the email address type
 *		  if no host is in the email address.
 * wanthost	- One of AD_HOST or AD_NHST.  If AD_HOST, look up the
 *		  "official name" of the host.  Well, that's what the
 *		  documentation says, at least ... support for that
 *		  functionality was removed when hostable support was
 *		  removed and the address parser was converted by default
 *		  to always being in DUMB mode.  So nowadays this only
 *		  affects where error messages are put if there is no
 *		  host part (set it to AD_HOST if you want error messages
 *		  to appear on standard error).
 * eresult	- Any error string returned by the address parser.  String
 *		  must contain sufficient room for the error message.
 *		  (BUFSIZ is used in general by the code).  Can be NULL.
 *
 * A pointer to an allocated struct mailname corresponding to the email
 * address is returned.
 */
struct mailname *getm(char *str, char *dfhost, int dftype,
		      int wanthost, char *eresult);
