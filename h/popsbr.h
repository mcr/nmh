
/*
 * popsbr.h -- header for POP client subroutines
 *
 * $Id$
 */

#if 0
#if !defined(NNTP) && defined(MPOP)
# define command pop_command
# define multiline pop_multiline
#endif
#endif

#ifdef NNTP
int pop_set (int, int, int, char *);
#else
int pop_set (int, int, int);
#endif

#ifdef NNTP
int pop_exists (int (*)());
#endif

int pop_init (char *, char *, char *, char *, int, int, int, int, char *);
int pop_fd (char *, int, char *, int);
int pop_stat (int *, int *);
int pop_retr (int, int (*)());
int pop_dele (int);
int pop_noop (void);
int pop_rset (void);
int pop_top (int, int, int (*)());
int pop_quit (void);
int pop_done (void);

#ifdef BPOP
int pop_list (int, int *, int *, int *, int *);
#else
int pop_list (int, int *, int *, int *);
#endif

#ifdef BPOP
int pop_xtnd (int (*)(), char *, ...);
#endif

#if defined(MPOP) && !defined(NNTP)
int pop_last (void);
#endif

#if !defined(NNTP) && defined(MPOP)
/* otherwise they are static functions */
int command(const char *, ...);
int multiline(void);
#endif

/*
 * Flags for the various pop authentication methods
 */
#define POP_APOP   -1
#define POP_PASSWD  0
#define POP_RPOP    1
#define POP_KPOP    2
