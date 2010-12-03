
/*
 * msh.h -- definitions for msh
 */

/* flags for stream */
#define	STDIO	0		/* regular stdoutput */
#define	CRTIO	1		/* create  re-direct */
#define	APPIO	2		/* append  re-direct */
#define	PIPIO	3		/* pipe    re-direct */

struct Cmd {
    char line[BUFSIZ];
    char *args[MAXARGS];
    char *redirect;
    int direction;
    FILE *stream;
};

#define	NULLCMD	((struct Cmd *) 0)

#define	MHNCHK 0x0001	/* did nontext check           */
#define	MHNYES 0x0002	/* .. and known to be non-text */

#define CUR (1 << (FFATTRSLOT + NUMATTRS - 1))

#ifdef BPOP
# define VIRTUAL SELECT_EMPTY

# define is_virtual(mp,msgnum)    ((mp)->msgstats[msgnum] & VIRTUAL)
# define unset_virtual(mp,msgnum) ((mp)->msgstats[msgnum] &= ~VIRTUAL)
# define set_virtual(mp,msgnum)   ((mp)->msgstats[msgnum] |= VIRTUAL)
#endif

struct Msg {
    struct drop m_drop;
    char *m_scanl;
    struct tws m_tb;
    short m_flags;
    seqset_t m_stats;
};

#define	m_bboard_id  m_drop.d_id
#define	m_top        m_drop.d_size
#define	m_start      m_drop.d_start
#define	m_stop       m_drop.d_stop

/*
 * FOLDER
 */
extern char *fmsh;		/* folder instead of file  */
extern int modified;		/* command modified folder */
extern struct msgs *mp;		/* used a lot              */
extern struct Msg *Msgs;	/* Msgs[0] not used        */

FILE *msh_ready ();

/*
 * COMMAND
 */
extern int interactive;		/* running from a /dev/tty */
extern int redirected;		/* re-directing output     */
extern FILE *sp;		/* original stdout         */
extern char *cmd_name;		/* command being run       */
extern char myfilter[];		/* path to mhl.forward     */

extern char *BBoard_ID;		/* BBoard-ID constant */

/*
 * SIGNALS
 */
extern SIGNAL_HANDLER istat;	/* original SIGINT  */
extern SIGNAL_HANDLER qstat;	/* original SIGQUIT */
extern int interrupted;		/* SIGINT detected  */
extern int broken_pipe;		/* SIGPIPE detected */
extern int told_to_quit;	/* SIGQUIT detected */

#ifdef BSD42
extern int should_intr;		/* signal handler should interrupt call */
extern jmp_buf sigenv;		/* the environment pointer */
#endif

/*
 * prototypes
 */
int readid (int);
int expand (char *);
void m_reset (void);
void fsetup (char *);
void setup (char *);
void readids (int);
void display_info (int);

void forkcmd (char **s, char *);
void distcmd (char **);
void explcmd (char **);
int filehak (char **);
void filecmd (char **);
void foldcmd (char **);
void forwcmd (char **);
void helpcmd (char **);
void markcmd (char **);
void mhncmd (char **);
void showcmd (char **);
int pack (char *, int, int);
int packhak (char **);
void packcmd (char **);
void pickcmd (char **);
void replcmd (char **);
void rmmcmd (char **);
void scancmd (char **);
void sortcmd (char **);
