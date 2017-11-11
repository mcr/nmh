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

char *concat (const char *, ...) ENDNULL;

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

char *nmh_getpass(const char *);
int pidwait (pid_t, int);
char *r1bindex(char *, int) PURE;
int refile (char **, char *);

/*
 * Read our credentials file and (optionally) ask the user for anything
 * missing.
 *
 * Arguments:
 *
 * host		- Hostname (to scan credentials file)
 * aname	- Pointer to filled-in username
 * apass	- Pointer to filled-in password
 * flags	- One or more of RUSERPASS_NO_PROMPT_USER,
 *                RUSERPASS_NO_PROMPT_PASSWORD
 */
void ruserpass (const char *host, char **aname, char **apass, int flags);
#define RUSERPASS_NO_PROMPT_USER	0x01
#define RUSERPASS_NO_PROMPT_PASSWORD	0x02

int remdir (char *);
void scan_detect_mbox_style (FILE *);
void scan_finished(void);

/*
 * Read the sequence files for the folder referenced in the given
 * struct msgs and populate the sequence entries in the struct msgs.
 *
 * Arguments:
 *
 * mp		- Folder structure to add sequence entries to
 * lockflag	- If true, obtain a write lock on the sequence file.
 *		  Additionally, the sequence file will remain open
 *		  and a pointer to the filehandle will be stored in
 *		  folder structure, where it will later be used by
 *		  seq_save().
 *
 * Return values:
 *     OK       - successfully read the sequence files, or they don't exist
 *     NOTOK    - failed to lock sequence file
 */
int seq_read (struct msgs * mp, int lockflag);
void seq_save (struct msgs *);
void seq_setcur (struct msgs *, int);
void seq_setprev (struct msgs *);
void seq_setunseen (struct msgs *, int);
int showfile (char **, char *);
int smatch(const char *, const struct swit *) PURE;

/*
 * Convert a set of bit flags to printable format.
 *
 * Arguments:
 *
 * buffer	- Buffer to output string to.
 * size		- Size of buffer in bytes.  Buffer is always NUL terminated.
 * flags	- Binary flags to output
 * bitfield	- Textual representation of bits to output.  This string
 *		  is in the following format:
 *
 *	Option byte 0x01 STRING1 0x02 STRING2 ....
 *
 * The first byte is an option byte to snprintb().  Currently the only option
 * supported is 0x08, which indicates that the flags should be output in
 * octal format; if the option byte is any other value, the flags will be
 * output in hexadecimal.
 *
 * After the option bytes are series of text strings, prefixed by the number
 * of the bit they correspond to.  For example, the bitfield string:
 *
 *	"\020\01FLAG1\02FLAG2\03FLAG3\04FLAG4"
 *
 * will output the following string if "flags" is set to 0x09:
 *
 *	0x2<FLAG1,FLAG4>
 *
 * You don't have to use octal in the bitfield string, that's just the
 * convention currently used by the nmh code.  The order of flags in the
 * bitfield string is not significant, but again, general convention is
 * from least significant bit to most significant.
 */
char *snprintb (char *buffer, size_t size, unsigned flags, char *bitfield);
int ssequal (const char *, const char *) PURE;
int stringdex (char *, char *) PURE;

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
