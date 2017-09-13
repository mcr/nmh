/* addrsbr.h -- definitions for the address parsing system
 */

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

/*
 * See notes for auxformat() below.
 */

#define	adrformat(m) auxformat ((m), 1)

/*
 *  prototypes
 */
void mnfree(struct mailname *);
bool ismymbox(struct mailname *);

/*
 * Enable Email Address Internationalization, which requires
 * support of 8-bit addresses.
 */
void enable_eai(void);

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

/*
 * Format an email address given a struct mailname.
 *
 * This function takes a pointer to a struct mailname and returns a pointer
 * to a static buffer holding the resulting email address.
 *
 * It is worth noting that group names are NOT handled, so if you want to
 * do something with groups you need to handle it externally to this function.
 *
 * Arguments include:
 *
 * mp		- Pointer to mailname structure
 * extras	- If true, include the personal name and/or note in the
 *		  address.  Otherwise, omit it.
 */

char *auxformat(struct mailname *mp, int extras);

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
 * eresult	- A buffer containing an error message returned by the
 *		  address parser.  May be NULL.
 * eresultsize	- The size of the buffer passed in eresult.
 *
 * A pointer to an allocated struct mailname corresponding to the email
 * address is returned.
 *
 * This function used to have an argument called 'wanthost' which would
 * control whether or not it would canonicalize hostnames in email
 * addresses.  This functionality was removed for nmh 1.5, and eventually
 * all of the code that used this argument was garbage collected.
 */
struct mailname *getm(char *str, char *dfhost, int dftype, char *eresult,
		      size_t eresultsize);
