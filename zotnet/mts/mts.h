
/*
 * mts.h -- definitions for the mail system
 *
 * $Id$
 */

/*
 * Local and UUCP Host Name
 */
char *LocalName(void);
char *SystemName(void);

/*
 * Mailboxes
 */
extern char *mmdfldir;
extern char *mmdflfil;
extern char *uucpldir;
extern char *uucplfil;

#define	MAILDIR	(mmdfldir && *mmdfldir ? mmdfldir : getenv ("HOME"))
#define	MAILFIL	(mmdflfil && *mmdflfil ? mmdflfil : getusername ())
#define	UUCPDIR	(uucpldir && *uucpldir ? uucpldir : getenv ("HOME"))
#define	UUCPFIL	(uucplfil && *uucplfil ? uucplfil : getusername ())

char *getusername(void);
char *getfullname(void);

/*
 * Separators
 */
extern char *mmdlm1;
extern char *mmdlm2;

#define	isdlm1(s) (strcmp (s, mmdlm1) == 0)
#define	isdlm2(s) (strcmp (s, mmdlm2) == 0)

/*
 * Read mts.conf file
 */
void mts_init (char *);

/*
 * MTS specific variables
 */
#if defined(SENDMTS) || defined (SMTPMTS)
extern char *hostable;
extern char *sendmail;
#endif

/*
 * SMTP/POP stuff
 */
extern char *clientname;
extern char *servers;
extern char *pophost;

/*
 * BBoards-specific variables
 */
extern char *bb_domain;

/*
 * POP BBoards-specific variables
 */
#ifdef BPOP
extern char *popbbhost;
extern char *popbbuser;
extern char *popbblist;
#endif /* BPOP */

/*
 * Global MailDelivery File
 */
extern char *maildelivery;

/*
 * Aliasing Facility (doesn't belong here)
 */
extern int Everyone;
extern char *NoShell;
