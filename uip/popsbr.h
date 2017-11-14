/* popsbr.h -- POP client subroutines
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

/* TLS flags */
#define P_INITTLS 0x01
#define P_NOVERIFY 0x02

int pop_init(char *, char *, char *, char *, int, int, char *, int, const char *);
int pop_stat(int *, int *);
int pop_retr(int, int (*)(void *, char *), void *);
int pop_dele(int);
int pop_quit(void);
int pop_done(void);
