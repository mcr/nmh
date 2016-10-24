
/*
 * nmh.h -- system configuration header file
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#ifndef NDEBUG
  /* See etc/gen-ctype-checked.c. */
# include <sbr/ctype-checked.h>
#endif
#include <assert.h>

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# define bool int
# define true 1
# define false 0
#endif

#include <sys/stat.h>
#include <sys/wait.h>

# include <dirent.h>
#define NLENGTH(dirent) strlen((dirent)->d_name)

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#include <locale.h>
#include <limits.h>
#include <errno.h>

/*
 * we should be getting this value from pathconf(_PC_PATH_MAX)
 */
#ifndef PATH_MAX
# ifdef MAXPATHLEN
#  define PATH_MAX MAXPATHLEN
# else
   /* so we will just pick something */
#  define PATH_MAX 1024
# endif
#endif

/*
 * we should get this value from sysconf(_SC_NGROUPS_MAX)
 */
#ifndef NGROUPS_MAX
# ifdef NGROUPS
#  define NGROUPS_MAX NGROUPS
# else
#  define NGROUPS_MAX 16
# endif
#endif

/*
 * we should be getting this value from sysconf(_SC_OPEN_MAX)
 */
#ifndef OPEN_MAX
# ifdef NOFILE
#  define OPEN_MAX NOFILE
# else
   /* so we will just pick something */
#  define OPEN_MAX 64
# endif
#endif

#ifndef HAVE_GETLINE
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif

/*
 * Defaults for programs if they aren't configured in a user's profile
 */

#define DEFAULT_PAGER "more"
#define DEFAULT_EDITOR "vi"
