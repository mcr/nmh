
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

/*
 * A vector of char array, used to hold a list of string message numbers
 * or command arguments.
 */

struct msgs_array {
	int max, size;
	char **msgs;
};

/*
 * Same as msgs_array, but for a vector of ints
 */

struct msgnum_array {
	int max, size;
	int *msgnums;
};

/*
 * Add a argument to the given msgs_array or msgnum_array structure; extend
 * the array size if necessary
 */

void app_msgarg(struct msgs_array *, char *);
void app_msgnum(struct msgnum_array *, int);

int open_form(char **, char *);
char *find_str (const char [], size_t, const char *);
char *rfind_str (const char [], size_t, const char *);
char *nmh_strcasestr (const char *, const char *);
