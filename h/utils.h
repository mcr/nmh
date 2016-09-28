
/*
 * utils.h -- utility prototypes
 */

void *mh_xmalloc(size_t);
void *mh_xrealloc(void *, size_t);
void *mh_xcalloc(size_t, size_t);
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

/*
 * See if a string contains 8 bit characters (use isascii() for the test).
 * Arguments include:
 *
 * start	- Pointer to start of string to test.
 * end		- End of string to test (test will stop before reaching
 *		  this point).  If NULL, continue until reaching '\0'.
 *
 * This function always stops at '\0' regardless of the value of 'end'.
 * Returns 1 if the string contains an 8-bit character, 0 if it does not.
 */
int contains8bit(const char *start, const char *end);

/*
 * Check to see if the user is running a different version of nmh
 * than they had last run, and notify them if so.  But only if
 * they seem to be running an interactive program.
 * Returns 1 if they are, 0 if they are not.
 */
int nmh_version_changed ();
