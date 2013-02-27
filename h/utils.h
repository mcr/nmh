
/*
 * utils.h -- utility prototypes
 */

void *mh_xmalloc(size_t);
void *mh_xrealloc(void *, size_t);
char *pwd(void);
char *add(const char *, char *);
char *addlist(char *, const char *);
int folder_exists(const char *);
void create_folder(char *, int, void (*)(int));
int num_digits(int);

struct msgs_array {
	int max, size;
	char **msgs;
};

void app_msgarg(struct msgs_array *, char *);
int open_form(char **, char *);
char *find_str (const char [], size_t, const char *);
char *rfind_str (const char [], size_t, const char *);
char *nmh_strcasestr (const char *, const char *);
