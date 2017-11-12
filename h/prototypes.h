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

char *m_name (int);

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
