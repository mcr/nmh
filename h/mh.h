
/*
 * mh.h -- main header file for all of nmh
 *
 * $Id$
 */

#include <h/nmh.h>

/*
 * Well-used constants
 */
#define	NOTOK        (-1)	/* syscall()s return this on error */
#define	OK             0	/*  ditto on success               */
#define	DONE           1	/* trinary logic                   */
#define ALL           ""
#define	Nbby           8	/* number of bits/byte */

#define MAXARGS	    1000	/* max arguments to exec                */
#define NFOLDERS    1000	/* max folder arguments on command line */
#define DMAXFOLDER     4	/* typical number of digits             */
#define MAXFOLDER   1000	/* message increment                    */

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
typedef unsigned char  boolean;  /* not int so we can pack in a structure */

/*
 * user context/profile structure
 */
struct node {
    char *n_name;		/* key                  */
    char *n_field;		/* value                */
    char  n_context;		/* context, not profile */
    struct node *n_next;	/* next entry           */
};

/*
 * switches structure
 */
#define	AMBIGSW	 (-2)	/* from smatch() on ambiguous switch */
#define	UNKWNSW	 (-1)	/* from smatch() on unknown switch   */

struct swit {
    char *sw;

    /* The minchars field is apparently used like this:

       -# : Switch can be abbreviated to # characters; switch hidden in -help.
       0  : Switch can't be abbreviated;               switch shown in -help.
       #  : Switch can be abbreviated to # characters; switch shown in -help. */
    int minchars;
};

extern struct swit anoyes[];	/* standard yes/no switches */

/*
 * general folder attributes
 */
#define READONLY   (1<<0)	/* No write access to folder    */
#define	SEQMOD	   (1<<1)	/* folder's sequences modifed   */
#define	ALLOW_NEW  (1<<2)	/* allow the "new" sequence     */
#define	OTHERS	   (1<<3)	/* folder has other files	*/
#define	MODIFIED   (1<<4)	/* msh in-core folder modified  */

#define	FBITS "\020\01READONLY\02SEQMOD\03ALLOW_NEW\04OTHERS\05MODIFIED"

/*
 * type for holding the sequence set of a message
 */
typedef unsigned int seqset_t;

/*
 * Determine the number of user defined sequences we
 * can have.  The first 5 sequence flags are for
 * internal nmh message flags.
 */
#define	NUMATTRS  ((sizeof(seqset_t) * Nbby) - 5)

/*
 * first free slot for user defined sequences
 * and attributes
 */
#define	FFATTRSLOT  5

/*
 * internal messages attributes (sequences)
 */
#define EXISTS        (1<<0)	/* exists            */
#define DELETED       (1<<1)	/* deleted           */
#define SELECTED      (1<<2)	/* selected for use  */
#define SELECT_EMPTY  (1<<3)	/* "new" message     */
#define	SELECT_UNSEEN (1<<4)	/* inc/show "unseen" */

#define	MBITS "\020\01EXISTS\02DELETED\03SELECTED\04NEW\05UNSEEN"

/*
 * Primary structure of folder/message information
 */
struct msgs {
    int lowmsg;		/* Lowest msg number                 */
    int hghmsg;		/* Highest msg number                */
    int nummsg;		/* Actual Number of msgs             */

    int lowsel;		/* Lowest selected msg number        */
    int hghsel;		/* Highest selected msg number       */
    int numsel;		/* Number of msgs selected           */

    int curmsg;		/* Number of current msg if any      */

    int msgflags;	/* Folder attributes (READONLY, etc) */
    char *foldpath;	/* Pathname of folder                */

    /*
     * Name of sequences in this folder.  We add an
     * extra slot, so we can NULL terminate the list.
     */
    char *msgattrs[NUMATTRS + 1];

    /*
     * bit flags for whether sequence
     * is public (0), or private (1)
     */
    seqset_t attrstats;

    /*
     * These represent the lowest and highest possible
     * message numbers we can put in the message status
     * area, without calling folder_realloc().
     */
    int	lowoff;
    int	hghoff;

    /*
     * This is an array of seqset_t which we allocate dynamically.
     * Each seqset_t is a set of bits flags for a particular message.
     * These bit flags represent general attributes such as
     * EXISTS, SELECTED, etc. as well as track if message is
     * in a particular sequence.
     */
    seqset_t *msgstats;		/* msg status */
};

/*
 * Amount of space to allocate for msgstats.  Allocate
 * the array to have space for messages numbers lo to hi.
 */
#define MSGSTATSIZE(mp,lo,hi) ((size_t) (((hi) - (lo) + 1) * sizeof(*(mp)->msgstats)))

/*
 * macros for message and sequence manipulation
 */
#define clear_msg_flags(mp,msgnum) ((mp)->msgstats[(msgnum) - mp->lowoff] = 0)
#define copy_msg_flags(mp,i,j) \
	((mp)->msgstats[(i) - mp->lowoff] = (mp)->msgstats[(j) - mp->lowoff])
#define get_msg_flags(mp,ptr,msgnum)  (*(ptr) = (mp)->msgstats[(msgnum) - mp->lowoff])
#define set_msg_flags(mp,ptr,msgnum)  ((mp)->msgstats[(msgnum) - mp->lowoff] = *(ptr))

#define does_exist(mp,msgnum)     ((mp)->msgstats[(msgnum) - mp->lowoff] & EXISTS)
#define unset_exists(mp,msgnum)   ((mp)->msgstats[(msgnum) - mp->lowoff] &= ~EXISTS)
#define set_exists(mp,msgnum)     ((mp)->msgstats[(msgnum) - mp->lowoff] |= EXISTS)

#define is_selected(mp,msgnum)    ((mp)->msgstats[(msgnum) - mp->lowoff] & SELECTED)
#define unset_selected(mp,msgnum) ((mp)->msgstats[(msgnum) - mp->lowoff] &= ~SELECTED)
#define set_selected(mp,msgnum)   ((mp)->msgstats[(msgnum) - mp->lowoff] |= SELECTED)

#define is_select_empty(mp,msgnum) ((mp)->msgstats[(msgnum) - mp->lowoff] & SELECT_EMPTY)
#define set_select_empty(mp,msgnum) \
	((mp)->msgstats[(msgnum) - mp->lowoff] |= SELECT_EMPTY)

#define is_unseen(mp,msgnum)      ((mp)->msgstats[(msgnum) - mp->lowoff] & SELECT_UNSEEN)
#define unset_unseen(mp,msgnum)   ((mp)->msgstats[(msgnum) - mp->lowoff] &= ~SELECT_UNSEEN)
#define set_unseen(mp,msgnum)     ((mp)->msgstats[(msgnum) - mp->lowoff] |= SELECT_UNSEEN)

/* for msh only */
#define set_deleted(mp,msgnum)    ((mp)->msgstats[(msgnum) - mp->lowoff] |= DELETED)

#define in_sequence(mp,seqnum,msgnum) \
           ((mp)->msgstats[(msgnum) - mp->lowoff] & (1 << (FFATTRSLOT + seqnum)))
#define clear_sequence(mp,seqnum,msgnum) \
           ((mp)->msgstats[(msgnum) - mp->lowoff] &= ~(1 << (FFATTRSLOT + seqnum)))
#define add_sequence(mp,seqnum,msgnum) \
           ((mp)->msgstats[(msgnum) - mp->lowoff] |= (1 << (FFATTRSLOT + seqnum)))

#define is_seq_private(mp,seqnum) \
           ((mp)->attrstats & (1 << (FFATTRSLOT + seqnum)))
#define make_seq_public(mp,seqnum) \
           ((mp)->attrstats &= ~(1 << (FFATTRSLOT + seqnum)))
#define make_seq_private(mp,seqnum) \
           ((mp)->attrstats |= (1 << (FFATTRSLOT + seqnum)))
#define make_all_public(mp) \
           ((mp)->attrstats = 0)

/*
 * macros for folder attributes
 */
#define clear_folder_flags(mp) ((mp)->msgflags = 0)

#define is_readonly(mp)     ((mp)->msgflags & READONLY)
#define set_readonly(mp)    ((mp)->msgflags |= READONLY)

#define other_files(mp)     ((mp)->msgflags & OTHERS)
#define set_other_files(mp) ((mp)->msgflags |= OTHERS)

#define	NULLMP	((struct msgs *) 0)

/*
 * m_getfld() message parsing
 */

#define NAMESZ  128		/* Limit on component name size     */

#define LENERR  (-2)		/* Name too long error from getfld  */
#define FMTERR  (-3)		/* Message Format error             */
#define FLD      0		/* Field returned                   */
#define FLDPLUS  1		/* Field returned with more to come */
#define FLDEOF   2		/* Field returned ending at eom     */
#define BODY     3		/* Body  returned with more to come */
#define BODYEOF  4		/* Body  returned ending at eom     */
#define FILEEOF  5		/* Reached end of input file        */

/*
 * Maildrop styles
 */
#define	MS_DEFAULT	0	/* default (one msg per file) */
#define	MS_UNKNOWN	1	/* type not known yet         */
#define	MS_MBOX		2	/* Unix-style "from" lines    */
#define	MS_MMDF		3	/* string mmdlm2              */
#define	MS_MSH		4	/* whacko msh                 */

extern int msg_count;		/* m_getfld() indicators */
extern int msg_style;		/*  .. */
extern char *msg_delim;		/*  .. */

#define	NOUSE	0		/* draft being re-used */

#define TFOLDER 0		/* path() given a +folder */
#define TFILE   1		/* path() given a file    */
#define	TSUBCWF	2		/* path() given a @folder */

#define OUTPUTLINELEN	72	/* default line length for headers */

/*
 * miscellaneous macros
 */
#define	pidXwait(pid,cp) pidstatus (pidwait (pid, NOTOK), stdout, cp)

#ifndef max
# define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef abs
# define abs(a) ((a) > 0 ? (a) : -(a))
#endif

/*
 * GLOBAL VARIABLES
 */
#define CTXMOD	0x01		/* context information modified */
#define	DBITS	"\020\01CTXMOD"
extern char ctxflags;

extern char *invo_name;		/* command invocation name         */
extern char *mypath;		/* user's $HOME                    */
extern char *defpath;		/* pathname of user's profile      */
extern char *ctxpath;		/* pathname of user's context      */
extern struct node *m_defs;	/* list of profile/context entries */

/*
 * These standard strings are defined in config.c.  They are the
 * only system-dependent parameters in nmh, and thus by redefining
 * their values and reloading the various modules, nmh will run
 * on any system.
 */
extern char *buildmimeproc;
extern char *catproc;
extern char *components;
extern char *context;
extern char *current;
extern char *defaulteditor;
extern char *defaultfolder;
extern char *digestcomps;
extern char *distcomps;
extern char *draft;
extern char *faceproc;
extern char *fileproc;
extern char *foldprot;
extern char *forwcomps;
extern char *inbox;
extern char *incproc;
extern char *installproc;
extern char *lproc;
extern char *mailproc;
extern char *mh_defaults;
extern char *mh_profile;
extern char *mh_seq;
extern char *mhlformat;
extern char *mhlforward;
extern char *mhlproc;
extern char *mhlreply;
extern char *moreproc;
extern char *msgprot;
extern char *mshproc;
extern char *nmhaccessftp;
extern char *nmhstorage;
extern char *nmhcache;
extern char *nmhprivcache;
extern char *nsequence;
extern char *packproc;
extern char *postproc;
extern char *pfolder;
extern char *psequence;
extern char *rcvdistcomps;
extern char *rcvstoreproc;
extern char *replcomps;
extern char *replgroupcomps;
extern char *rmfproc;
extern char *rmmproc;
extern char *sendproc;
extern char *showmimeproc;
extern char *showproc;
extern char *usequence;
extern char *version_num;
extern char *version_str;
extern char *vmhproc;
extern char *whatnowproc;
extern char *whomproc;

#include <h/prototypes.h>

