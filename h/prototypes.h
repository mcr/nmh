/* prototypes.h -- various prototypes
 *
 * If you modify functions here, please document their current behavior
 * as much as practical.
 */

/*
 * prototype from config.h
 */
char *etcpath(char *) NONNULL(1);

/*
 * prototypes from the nmh subroutine library
 */

struct msgs_array;

/*
 * Print null-terminated PROMPT to and flush standard output.  Read answers from
 * standard input until one matches an entry in SWITCHES.  When one matches,
 * return its swret field.  Return 0 on EOF.
 */
int read_switch(const char *PROMPT, const struct swit *SWITCHES);

/*
 * If standard input is not a tty, return 1 without printing anything.  Else,
 * print null-terminated PROMPT to and flush standard output.  Read answers from
 * standard input until one is "yes" or "no", returning 1 for "yes" and 0 for
 * "no".  Also return 0 on EOF.
 */
int read_yes_or_no_if_tty(const char *PROMPT);

/*
 * Print null-terminated PROMPT to and flush standard output.  Read multi-word
 * answers from standard input until a first word matches an entry in SWITCHES.
 * When one matches, return a pointer to an array of pointers to the words.
 * Return NULL on EOF, interrupt, or other error.
 */
char **read_switch_multiword(const char *PROMPT, const struct swit *SWITCHES);

/*
 * Same as read_switch_multiword but using readline(3) for input.
 */
#ifdef READLINE_SUPPORT
char **read_switch_multiword_via_readline (char *, struct swit *);
#endif /* READLINE_SUPPORT */

char **getarguments (char *, int, char **, int);

m_getfld_state_t m_getfld_state_init(FILE *iob);
void m_getfld_state_reset (m_getfld_state_t *);
void m_getfld_state_destroy (m_getfld_state_t *);
void m_getfld_track_filepos (m_getfld_state_t *, FILE *);
void m_getfld_track_filepos2(m_getfld_state_t *);
int m_getfld (m_getfld_state_t *, char[NAMESZ], char *, int *, FILE *);
int m_getfld2(m_getfld_state_t *, char[NAMESZ], char *, int *);
int m_gmprot (void) PURE;
char *m_name (int);

void m_unknown(m_getfld_state_t *, FILE *);
void m_unknown2(m_getfld_state_t *);

int pidwait (pid_t, int);

void scan_detect_mbox_style (FILE *);
void scan_finished(void);

/*
 * prototypes for some routines in uip
 */
int annotate (char *, char *, char *, bool, bool, int, bool);
void annolist(char *, char *, char *, int);
void annopreserve(int);
int mhl(int, char **);
int mhlsbr(int, char **, FILE *(*)(char *));
int distout (char *, char *, char *);
int sendsbr (char **, int, char *, char *, struct stat *, int, const char *);
int what_now (char *, int, int, char *, char *,
	int, struct msgs *, char *, int, char *, int);
int WhatNow(int, char **) NORETURN;
