#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* mh.h - main header file for all of mh */

/*  VOID is used to indicate explicitly that the value of a function
/*  is to be ignored.
/*  If you have a newer C compiler, it is better to simply say:
       #define VOID (void)
/*  instead of what follows
/**/
# define void int
# ifdef  lint
# define VOID _VOID_ = (int)
  int     _VOID_;
# else
# define VOID
# endif

/* #define ARPANET         /* if you are on the ARPANET */

/* #define VMUNIX          /* if you are running a Berkeley system */
/* if you define VMUNIX, then define JOBSLIB=-ljobs in progs/Makefile */

#ifdef ARPANET
#define NOGATEWAY
#ifdef  NOGATEWAY
  extern char *rhosts[];
#endif  NOGATEWAY
#endif ARPANET

#define ALL ""
#define NULLCP (char *)0

#define MAXFOLDER 999   /* Max number of messages in a folder */
#define DMAXFOLDER 3    /* Number of digits in MAXFOLDER */

#define MAXARGS 1000    /* Max messages to exec                 */

			/* Flag bits in msgstats                        */
#define EXISTS    01    /* Message exists                               */
#define DELETED   02    /* Deleted undefined currently                  */
#define SELECTED  04    /* Message selected by an arg                   */
#define SELECT_EMPTY 010/* Single empty msg selected by an mhpath arg   */

#define READONLY  01    /* No write access to folder                    */
#define DEFMOD    01    /* In-core profile has been modified            */

/*#define NEWS     1    /* Define for news inclusion                    */

struct  swit {
	char *sw;
	int minchars;
};

/*
 * m_gmsg() returns this structure.  It contains the per folder
 * information which is obtained from reading the folder directory.
 */

struct  msgs {
	int     hghmsg;         /* Highest msg in directory     */
	int     nummsg;         /* Actual Number of msgs        */
	int     lowmsg;         /* Lowest msg number            */
	int     curmsg;         /* Number of current msg if any */
	int     lowsel;         /* Lowest selected msg number   */
	int     hghsel;         /* Highest selected msg number  */
	int     numsel;         /* Number of msgs selected      */
	char   *foldpath;       /* Pathname of folder           */
	char    selist,         /* Folder has a "select" file   */
		msgflags,       /* Folder status bits           */
		filler,
		others;         /* Folder has other file(s)     */
	char    msgstats[1];    /* Stat bytes for each msg      */
};

		/* m_getfld definitions and return values       */

#define NAMESZ  64      /* Limit on component name size         */
#define LENERR  -2      /* Name too long error from getfld      */
#define FMTERR  -3      /* Message Format error                 */

			/* m_getfld return codes                */
#define FLD      0      /* Field returned                       */
#define FLDPLUS  1      /* Field " with more to come            */
#define FLDEOF   2      /* Field " ending at eom                */
#define BODY     3      /* Body  " with more to come            */
#define BODYEOF  4      /* Body  " ending at eom                */
#define FILEEOF  5      /* Reached end of input file            */

/*
 * These standard strings are defined in strings.a.  They are the
 * only system-dependent parameters in MH, and thus by redefining
 * their values and reloading the various modules, MH will run
 * on any system.
 */

extern char
	*components,    /* Name of user's component file (in mh dir) */
	*current,       /* Name of current msg file in a folder */
	*defalt,        /* Name of the std folder (inbox)       */
	*distcomps,     /* Name of `dist' components file       */
	*draft,         /* Name of the normal draft file        */
	*fileproc,      /* Path of file program                 */
	*foldprot,      /* Default folder protection            */
/*      *hostname,      /* Local net host name                  */
	*installproc,   /* Name of auto-install program path    */
/*      *layout,        /* Name of mhl layout file              */
	*listname,      /* Default selection list folder name   */
	*lockdir,       /* Dir for lock files (Same fs as mailboxes)*/
	*lproc,         /* Path of "list" prog for "What now?"  */
	*lsproc,        /* Path of the block style ls program   */
	*mailboxes,     /* Incoming mail directory              */
	*mailproc,      /* Path of Bell equivalent mail pgm, used by news -send */
	*mh_prof,       /* Name of users profile file           */
	*mh_deliver,    /* Name of deliverer for mh             */
	*mhlformat,     /* Name of mhl format file in MH dir    */
	*mhlstdfmt,     /* Name of standard mhl format file     */
	*mhnews,        /* Name of MH news file                 */
	*msgprot,       /* Default message protection (s.a. 0664) */
	*pfolder,       /* Name of current folder profile entry */
	*prproc,        /* Path of the pr program               */
	*scanproc,      /* Path of the scan program             */
	*showproc,      /* Path of the type (l) program         */
	*sendproc,      /* Path of the send message program     */
	*stdcomps,      /* Std comp file if missing user's own  */
	*stddcomps,     /* Std dist file if missing user's own  */
	*sysed;         /* Path of the std (e) editor           */

/* Just about every program uses this also via m_getdefs        */
char    *mypath;        /* User's log-on path                   */

/*
 * node structure used to hold a linked list of the users profile
 * information taken from logpath/.mh_prof.
 */

struct node {
	struct node *n_next;
	char        *n_name,
		    *n_field;
} *m_defs;

char  def_flags;


/*
 * The first char in the mhnews file indicates whether the program
 * calling m_news() should continue running or halt.
 */

#define NEWSHALT        '!'     /* Halt after showing the news  */
#define NEWSCONT        ' '     /* Continue  (ditto)            */
#define NEWSPAUSE       '\001'  /* Pause during news output...  */


/*
 * Miscellaneous Defines to speed things up
 */

#define error(str) { fprintf(stderr, "%s\n", str); exit(-1); }

/*
 * Routine type declarations -- needed by version 7 compiler
 */

char *invo_name;
char **brkstring();
char *m_maildir();
char *m_find();
char *m_name();
char *concat();
char *getcpy();
char *trimcpy();
char *add();
char **copyip();
char *getcpy();
char *m_getfolder();
struct msgs *m_gmsg();
char *copy();
char **getans();
char *cdate();
char *makename();
char *r1bindex();
char *pwd();
char *path();

/*
 * Routine type declarations -- SHOULD BE GLOBAL
 */
char *getenv();

/*
 * Defines for path evaluation type--routine path second arg:
 */

#define TFOLDER 0
#define TF