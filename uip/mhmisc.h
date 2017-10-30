/* mhmisc.h -- misc routines to process MIME messages
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

int part_ok(CT) PURE;
int part_exact(CT) PURE;
int type_ok(CT, int);
int is_inline(CT) PURE;
int make_intermediates(char *);
void content_error(char *, CT, char *, ...) CHECK_PRINTF(3, 4);
void flush_errors(void);

#define	NPARTS 50
#define	NTYPES 20

extern int npart;
extern int ntype;
extern char *parts[NPARTS + 1];
extern char *types[NTYPES + 1];

extern bool userrs;
