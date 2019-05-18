/* utils.h -- utility prototypes
 */

/* PLURALS gives a pointer to the string "s" when n isn't 1, and to the
 * empty string "" when it is.  Suitable for obtaining the plural `s'
 * used for English nouns.  It treats -1 as plural, as does GNU gettext.
 * Having output vary for plurals is annoying for those writing parsers;
 * better to phrase the output such that no test is needed, e.g.
 * "messages found: 42". */
extern const char plurals[];
#define PLURALS(n) (plurals + ((n) == 1))

/* Call malloc(3), exiting on NULL return. */
void *mh_xmalloc(size_t size) MALLOC ALLOC_SIZE(1);

/* Call realloc(3), exiting on NULL return. */
void *mh_xrealloc(void *ptr, size_t size) ALLOC_SIZE(2);

/* Call calloc(3), exiting on NULL return. */
void *mh_xcalloc(size_t nelem, size_t elsize) MALLOC ALLOC_SIZE(1, 2);

/* Duplicate a NUL-terminated string, exit on failure. */
char *mh_xstrdup(const char *src) MALLOC;

/* Set p to point to newly allocated, uninitialised, memory. */
#define NEW(p) ((p) = mh_xmalloc(sizeof *(p)))

/* Set p to point to newly allocated, zeroed, memory. */
#define NEW0(p) ((p) = mh_xcalloc(1, sizeof *(p)))

/* Zero the bytes to which p points. */
#define ZERO(p) memset((p), 0, sizeof *(p))

char *pwd(void);
char *add(const char *, char *) MALLOC;
char *addlist(char *, const char *) MALLOC;
int folder_exists(const char *);
void create_folder(char *, int, void (*)(int));
int num_digits(int) PURE;

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

char *find_str (const char [], size_t, const char *) PURE;
char *rfind_str (const char [], size_t, const char *) PURE;
char *nmh_strcasestr (const char *, const char *) PURE;

void trunccpy(char *dst, const char *src, size_t size);
/* A convenience for the common case of dst being an array. */
#define TRUNCCPY(dst, src) trunccpy(dst, src, sizeof (dst))

bool has_prefix(const char *s, const char *prefix) PURE;
bool has_suffix(const char *s, const char *suffix) PURE;
bool has_suffix_c(const char *s, int c) PURE;
void trim_suffix_c(char *s, int c);
void to_lower(char *s);
void to_upper(char *s);

bool contains8bit(const char *start, const char *end);

/*
 * See if file has any 8-bit bytes.
 * Arguments include:
 *
 * fd		- file descriptor
 * eightbit	- address of result, will be set to 1 if the file contains
 *                any 8-bit bytes, 0 otherwise.
 *
 * Returns OK on success, NOTOK on read failure.
 *
 */
int scan_input (int fd, int *eightbit);

/*
 * Returns string representation of int, in static memory.
 */
char *m_str(int value);

/*
 * Returns string representation of an int, in static memory.  If width
 * == 0, does not limit the width.  If width > 0 and value will not fit
 * in field of that size, including any negative sign but excluding
 * terminating null, then returns "?".
 */
char *m_strn(int value, unsigned int width);

/*
 * program initialization
 *
 * argv0        - argv[0], presumably the program name
 * read_context - whether to read the context
 * check_version - if read_context, whether to check the version, and issue warning message if non-existent or old
 */
int nmh_init(const char *argv0, bool read_context, bool check_version);

/*
 * Compares prior version of nmh with current version.  Returns 1
 * if they compare the be the same, 0 if not.
 *
 * older        - 0 for difference comparison, 1 for only if older
 */
int nmh_version_changed (int older);
