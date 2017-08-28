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

void add_profile_entry (const char *, const char *);
void inform(char *fmt, ...) CHECK_PRINTF(1, 2);
void adios (const char *, const char *, ...) CHECK_PRINTF(2, 3) NORETURN;
void die(const char *, ...) CHECK_PRINTF(1, 2) NORETURN;
void admonish (char *, char *, ...) CHECK_PRINTF(2, 3);
void advertise (const char *, char *, const char *, va_list) CHECK_PRINTF(3, 0);
void advise (const char *, const char *, ...) CHECK_PRINTF(2, 3);
char **argsplit (char *, char **, int *) NONNULL(1, 2);
void argsplit_msgarg (struct msgs_array *, char *, char **) NONNULL(1, 2, 3);
void argsplit_insert (struct msgs_array *, char *, char **) NONNULL(1, 2, 3);
void arglist_free (char *, char **);
void ambigsw (const char *, const struct swit *) NONNULL(1, 2);
int atooi(char *) NONNULL(1) PURE;
char **brkstring (char *, char *, char *) NONNULL(1);

/*
 * Check to see if we can display a given character set natively.
 * Arguments include:
 *
 * str	- Name of character set to check against
 * len	- Length of "str"
 *
 * Returns 1 if the specified character set can be displayed natively,
 * 0 otherwise.
 */

int check_charset (char *, int);
int client(char *, char *, char *, int, int);
void closefds(int);
char *concat (const char *, ...);
int context_del (char *);
char *context_find (const char *) PURE;
char *context_find_by_type (const char *, const char *, const char *);
int context_find_prefix(const char *) PURE;
int context_foil (char *);
void context_read (void);
void context_replace (char *, char *);
void context_save (void);
char **copyip (char **, char **, int);
void cpydata (int, int, const char *, const char *);
void cpydgst (int, int, char *, char *);
char *cpytrim (const char *);
char *rtrim (char *);
int decode_rfc2047 (char *, char *, size_t);
void discard (FILE *);

/*
 * Decode two characters into their quoted-printable representation.
 *
 * Arguments are:
 *
 * byte1	- First character of Q-P representation
 * byte2	- Second character of Q-P representation
 *
 * Returns the decoded value, -1 if the conversion failed.
 */
int decode_qp(unsigned char byte1, unsigned char byte2) CONST;

int default_done (int);

/*
 * Encode a message header using RFC 2047 encoding.  If the message contains
 * no non-ASCII characters, then leave the header as-is.
 *
 * Arguments include:
 *
 * name		- Message header name
 * value	- Message header content; must point to allocated memory
 *		  (may be changed if encoding is necessary)
 * encoding	- Encoding type.  May be one of CE_UNKNOWN (function chooses
 *		  the encoding), CE_BASE64 or CE_QUOTED
 * charset	- Charset used for encoding.  If NULL, obtain from system
 *		  locale.
 *
 * Returns 0 on success, any other value on failure.
 */

int encode_rfc2047(const char *name, char **value, int encoding,
		   const char *charset);

void escape_display_name (char *, size_t);
void escape_local_part (char *, size_t);
int ext_hook(char *, char *, char *);
int fdcompare (int, int);
int folder_addmsg (struct msgs **, char *, int, int, int, int, char *);
int folder_delmsgs (struct msgs *, int, int);
void folder_free (struct msgs *);
int folder_pack (struct msgs **, int);

/*
 * Read a MH folder structure and return an allocated "struct msgs"
 * corresponding to the contents of the folder.
 *
 * Arguments include:
 *
 * name		- Name of folder
 * lockflag	- If true, write-lock (and keep open) metadata files.
 *		  See comments for seq_read() for more information.
 */
struct msgs *folder_read (char *name, int lockflag);

struct msgs *folder_realloc (struct msgs *, int, int);

/*
 * Flush standard output, read a line from standard input into a static buffer,
 * zero out the newline, and return a pointer to the buffer.
 * On error, return NULL.
 */
const char *read_line(void);

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

/*
 * Returns the MIME character set indicated by the current locale setting.
 * Should be used by routines that want to convert to/from the local
 * character set, or if you want to check to see if you can display content
 * in the local character set.
 */
char *get_charset(void);

/* Return malloc'd copy of str, or of "" if NULL, exit on failure. */
char *getcpy(const char *str);

char *get_default_editor(void);
char *getfolder(int) PURE;

int m_atoi (char *) PURE;
char *m_backup (const char *);
int m_convert (struct msgs *, char *);
char *m_draft (char *, char *, int, int *);

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
char *new_fs (char *, char *, char *);
char *path(char *, int);
int pidwait (pid_t, int);
int pidstatus (int, FILE *, char *);
char *pluspath(char *);
void print_help (char *, struct swit *, int);
void print_intro (FILE *, int);
void print_sw (const char *, const struct swit *, char *, FILE *);
void print_version (char *);
void push (void);
char *r1bindex(char *, int) PURE;
void readconfig (struct node **, FILE *, const char *, int);
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
int seq_addmsg (struct msgs *, char *, int, int, int);
int seq_addsel (struct msgs *, char *, int, int);
char *seq_bits (struct msgs *);
int seq_delmsg (struct msgs *, char *, int);
int seq_delsel (struct msgs *, char *, int, int);
int seq_getnum (struct msgs *, char *);
char *seq_list (struct msgs *, char *);
int seq_nameok (char *);
void seq_print (struct msgs *, char *);
void seq_printall (struct msgs *);

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
char *trimcpy (char *);

int uprf (const char *, const char *) PURE;
int vfgets (FILE *, char **);

/*
 * Output the local character set name, but make sure it is suitable for
 * 8-bit characters.
 */
char *write_charset_8bit (void);


/*
 * some prototypes for address parsing system
 * (others are in addrsbr.h)
 */
char *LocalName(int);
char *SystemName(void);

/*
 * prototypes for some routines in uip
 */
int annotate (char *, char *, char *, bool, bool, int, bool);
void annolist(char *, char *, char *, int);
void annopreserve(int);
void m_pclose(void);
int mhl(int, char **);
int mhlsbr(int, char **, FILE *(*)(char *));
int distout (char *, char *, char *);
void replout (FILE *, char *, char *, struct msgs *, int,
	int, char *, char *, char *, int);
int build_form (char *, char *, int *, char *, char *, char *, char *,
		char *, char *);
int sendsbr (char **, int, char *, char *, struct stat *, int, const char *);
int what_now (char *, int, int, char *, char *,
	int, struct msgs *, char *, int, char *, int);
int WhatNow(int, char **) NORETURN;

/* Includes trailing NUL */

#define BASE64SIZE(x) ((((x + 2) / 3) * 4) + 1)

/*
 * Copy data from one file to another, converting to base64-encoding.
 *
 * Arguments include:
 *
 * in		- Input filehandle (unencoded data)
 * out		- Output filename (base64-encoded data)
 * crlf		- If set, output encoded CRLF for every LF on input.
 *
 * Returns OK on success, NOTOK otherwise.
 */
int writeBase64aux(FILE *in, FILE *out, int crlf);

int writeBase64 (const unsigned char *, size_t, unsigned char *);
int writeBase64raw (const unsigned char *, size_t, unsigned char *);

/*
 * encoded      - the string to be decoded
 * decoded      - the decoded bytes
 * len          - number of decoded bytes
 * skip-crs     - non-zero for text content, and for which CR's should be
 *                skipped
 * digest       - for an MD5 digest, it can be null
 */
int decodeBase64 (const char *encoded, unsigned char **decoded, size_t *len,
                  int skip_crs, unsigned char *digest);

void hexify (const unsigned char *, size_t, char **);

/*
 * credentials management
 */
void init_credentials_file(void);

/*
 * Allocate and return a credentials structure.  The credentials structure
 * is now opaque; you need to use accessors to get inside of it.  The
 * accessors will only prompt the user for missing fields if they are
 * needed.
 *
 * Arguments:
 *
 * host	- Hostname we're connecting to (used to search credentials file)
 * user	- Username we are logging in as; can be NULL.
 *
 * Returns NULL on error, otherwise an allocated nmh_creds structure.
 */
nmh_creds_t nmh_get_credentials (const char *host, const char *user);

/*
 * Retrieve the user from a nmh_creds structure.  May prompt the user
 * if one is not defined.
 *
 * Arguments:
 *
 * creds	- Structure from previous nmh_get_credentials() call
 *
 * Returns NULL on error, otherwise a NUL-terminated string containing
 * the username.  Points to allocated memory in the credentials structure
 * that is free()d by nmh_free_credentials().
 */
const char *nmh_cred_get_user(nmh_creds_t creds);

/*
 * Retrieve the password from an nmh_creds structure.  Otherwise identical
 * to nmh_cred_get_user().
 */
const char *nmh_cred_get_password(nmh_creds_t creds);

/*
 * Free an allocated nmh_creds structure.
 */
void nmh_credentials_free(nmh_creds_t creds);
