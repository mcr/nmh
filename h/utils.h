
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
int num_digits(int);

struct msgs_array {
	int max, size;
	char **msgs;
};

void app_msgarg(struct msgs_array *, char *);
int open_form(char **, char *);
