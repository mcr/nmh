/* dtime.h -- time/date routines
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

extern char *tw_moty[];
extern char *tw_dotw[];
extern char *tw_ldotw[];

struct tws *dlocaltimenow(void);
struct tws *dlocaltime(time_t *);
struct tws *dgmtime(time_t *);
char *dctime(struct tws *);
char *dtimenow(int);
char *dtime(time_t *, int);
char *dasctime(struct tws *, int);
char *dtimezone(int, int);
time_t dmktime(struct tws *);
void set_dotw(struct tws *);
int twsort(struct tws *, struct tws *);
