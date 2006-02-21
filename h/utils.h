
/*
 * utils.h -- utility prototypes
 *
 * $Id$
 */

void *mh_xmalloc(size_t);
void *mh_xrealloc(void *, size_t);
char *pwd(void);
char *add(char *, char *);
void create_folder(char *, int, void (*)());
