/* mts.h -- definitions for the mail system
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
extern char *spoollocking;

#define	MAILDIR	(mmdfldir && *mmdfldir ? mmdfldir : getenv ("HOME"))
#define	MAILFIL	(mmdflfil && *mmdflfil ? mmdflfil : getusername (1))

char *getusername(int);
char *getfullname(void);
char *getlocalmbox(void);

/*
 * Separators
 */
#define MMDF_DELIM "\001\001\001\001\n"

/*
 * Read mts.conf file
 */
void mts_init (void);

/*
 * MTS specific variables
 */

/* whether to speak SMTP, and over the network or directly to sendmail */
#define MTS_SMTP          0
#define MTS_SENDMAIL_SMTP 1
#define MTS_SENDMAIL_PIPE 2
extern int sm_mts;
void save_mts_method (const char *);

extern char *sendmail;

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
