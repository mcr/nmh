
/*
 * nmh.h -- system configuration header file
 *
 * $Id$
 */

#include <config.h>

#ifdef HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NLENGTH(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NLENGTH(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <stdarg.h>

#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
/* An ANSI string.h and pre-ANSI memory.h might conflict.  */
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#else   /* not STDC_HEADERS and not HAVE_STRING_H */
# include <strings.h>
/* memory.h and strings.h conflict on some systems.  */
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

/*
 * symbolic constants for lseek and fseek
 */
#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_CUR
# define SEEK_CUR 1
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif

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

#include <signal.h>
 
#define bcmp(b1,b2,length)      memcmp(b1, b2, length)
#define bcopy(b1,b2,length)     memcpy (b2, b1, length)
#define bcpy(b1,b2,length)      memcmp (b1, b2, length)
#define bzero(b,length)         memset (b, 0, length)

#ifdef HAVE_KILLPG
# define KILLPG(pgrp,sig) killpg(pgrp,sig);
#else
# define KILLPG(pgrp,sig) kill((-pgrp),sig);
#endif

/*
 * If your stat macros are broken,
 * we will just undefine them.
 */
#ifdef STAT_MACROS_BROKEN
# ifdef S_ISBLK
#  undef S_ISBLK
# endif 
# ifdef S_ISCHR
#  undef S_ISCHR
# endif 
# ifdef S_ISDIR
#  undef S_ISDIR
# endif 
# ifdef S_ISFIFO
#  undef S_ISFIFO
# endif 
# ifdef S_ISLNK
#  undef S_ISLNK
# endif 
# ifdef S_ISMPB
#  undef S_ISMPB
# endif 
# ifdef S_ISMPC
#  undef S_ISMPC
# endif 
# ifdef S_ISNWK
#  undef S_ISNWK
# endif 
# ifdef S_ISREG
#  undef S_ISREG
# endif 
# ifdef S_ISSOCK
#  undef S_ISSOCK
# endif 
#endif  /* STAT_MACROS_BROKEN.  */

