
/*
 * prototypes.h -- various prototypes
 *
 * $Id$
 */

/*
 * missing system prototypes
 */
#ifndef HAVE_TERMCAP_H
extern int tgetent (char *bp, char *name);
extern int tgetnum (char *id);
extern int tgetflag (char *id);
extern char *tgetstr (char *id, char **area);
extern char *tgoto (char *cm, int destcol, int destline);
extern int tputs (char *cp, int affcnt, int (*outc) (int));
#endif

/*
 * prototype from config.h
 */
char *etcpath(char *);

/*
 * prototypes from the nmh subroutine library
 */
char *add (char *, char *);
void adios (char *, char *, ...);
void admonish (char *, char *, ...);
void advertise (char *, char *, char *, va_list);
void advise (char *, char *, ...);
void ambigsw (char *, struct swit *);
int atooi(char *);
char **brkstring (char *, char *, char *);
int check_charset (char *, int);
void closefds(int);
char *concat (char *, ...);
int context_del (char *);
char *context_find (char *);
int context_foil (char *);
void context_read (void);
void context_replace (char *, char *);
void context_save (void);
char *copy (char *, char *);
char **copyip (char **, char **, int);
void cpydata (int, int, char *, char *);
void cpydgst (int, int, char *, char *);
int decode_rfc2047 (char *, char *);
void discard (FILE *);
int done (int);
int ext_hook(char *, char *, char *);
int fdcompare (int, int);
int folder_addmsg (struct msgs **, char *, int, int, int, int, char *);
int folder_delmsgs (struct msgs *, int, int);
void folder_free (struct msgs *);
int folder_pack (struct msgs **, int);
struct msgs *folder_read (char *);
struct msgs *folder_realloc (struct msgs *, int, int);
int gans (char *, struct swit *);
char **getans (char *, struct swit *);
int getanswer (char *);
char **getarguments (char *, int, char **, int);
char *getcpy (char *);
char *getfolder(int);
int lkclose(int, char*);
int lkfclose(FILE *, char *);
FILE *lkfopen(char *, char *);
int lkopen(char *, int, mode_t);
int m_atoi (char *);
char *m_backup (char *);
int m_convert (struct msgs *, char *);
char *m_draft (char *, char *, int, int *);
void m_eomsbr (int (*)());
int m_getfld (int, unsigned char *, unsigned char *, int, FILE *);
int m_gmprot (void);
char *m_maildir (char *);
char *m_mailpath (char *);
char *m_name (int);
int m_putenv (char *, char *);
char *m_scratch (char *, char *);
char *m_tmpfil (char *);
void m_unknown(FILE *);
int makedir (char *);
char *nmh_getpass(const char *);
char *new_fs (char *, char *, char *);
char *path(char *, int);
int peekc(FILE *ib);
int pidwait (pid_t, int);
int pidstatus (int, FILE *, char *);
void print_help (char *, struct swit *, int);
void print_sw (char *, struct swit *, char *);
void print_version (char *);
void push (void);
char *pwd (void);
char *r1bindex(char *, int);
void readconfig (struct node **, FILE *, char *, int);
int refile (char **, char *);
int ruserpass(char *, char **, char **);
int remdir (char *);
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
void seq_read (struct msgs *);
void seq_save (struct msgs *);
void seq_setcur (struct msgs *, int);
void seq_setprev (struct msgs *);
void seq_setunseen (struct msgs *, int);
int showfile (char **, char *);
int smatch(char *, struct swit *);
char *snprintb (char *, size_t, unsigned, char *);
int ssequal (char *, char *);
int stringdex (char *, char *);
char *trimcpy (char *);
int unputenv (char *);
int uprf (char *, char *);
int vfgets (FILE *, char **);
char *write_charset_8bit (void);

#ifdef RPATHS
int get_returnpath (char *, int, char *, int);
#endif

/*
 * prototypes for compatibility functions in library
 */
#ifndef HAVE_SNPRINTF_PROTOTYPE
int snprintf (char *, size_t, const char *, ...);
int vsnprintf (char *, size_t, const char *, va_list);
#endif

int strcasecmp (const char *s1, const char *s2);
int strncasecmp (const char *s1, const char *s2, size_t n);

#ifndef HAVE_STRERROR
char *strerror (int);
#endif


/*
 * some prototypes for address parsing system
 * (others are in addrsbr.h)
 */
char *LocalName(void);
char *SystemName(void);
char *OfficialName(char *);

/*
 * prototypes for some routines in uip
 */
int annotate (char *, char *, char *, int, int, int, int);
void annolist(char *, char *, char *, int);
int distout (char *, char *, char *);
void replout (FILE *, char *, char *, struct msgs *, int,
	int, char *, char *, char *);
int sendsbr (char **, int, char *, struct stat *, int, char *);
int what_now (char *, int, int, char *, char *,
	int, struct msgs *, char *, int, char *);

