
/*
 * mts.h -- definitions for the mail system
 */

/*
 * Local and UUCP Host Name
 */
char *LocalName(int);
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
char *getlocalmbox(void);

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
#if defined (SMTPMTS)

/* whether to speak SMTP to localhost:25 or to /usr/sbin/sendmail */
#define MTS_SMTP     0
#define MTS_SENDMAIL 1
extern int sm_mts;

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
 * Global MailDelivery File
 */
extern char *maildelivery;

/*
 * Aliasing Facility (doesn't belong here)
 */
extern int Everyone;
extern char *NoShell;
