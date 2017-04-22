/* ctype-checked.h -- ctype.h with compiler checks.
 *
 * This code is Copyright (c) 2016, by the authors of nmh.
 * See the COPYRIGHT file in the root directory of the nmh
 * distribution for complete copyright information. */

#ifndef CTYPE_CHECKED_H
#define CTYPE_CHECKED_H
#include <ctype.h>

extern int ctype_identity[257];

#define CTYPE_CHECK(f, c) ((f)((ctype_identity + 1)[c]))

#ifdef isalnum
#undef isalnum
#endif
#define isalnum(c) CTYPE_CHECK(isalnum, c)

#ifdef isalpha
#undef isalpha
#endif
#define isalpha(c) CTYPE_CHECK(isalpha, c)

#ifdef iscntrl
#undef iscntrl
#endif
#define iscntrl(c) CTYPE_CHECK(iscntrl, c)

#ifdef isdigit
#undef isdigit
#endif
#define isdigit(c) CTYPE_CHECK(isdigit, c)

#ifdef isgraph
#undef isgraph
#endif
#define isgraph(c) CTYPE_CHECK(isgraph, c)

#ifdef islower
#undef islower
#endif
#define islower(c) CTYPE_CHECK(islower, c)

#ifdef isprint
#undef isprint
#endif
#define isprint(c) CTYPE_CHECK(isprint, c)

#ifdef ispunct
#undef ispunct
#endif
#define ispunct(c) CTYPE_CHECK(ispunct, c)

#ifdef isspace
#undef isspace
#endif
#define isspace(c) CTYPE_CHECK(isspace, c)

#ifdef isupper
#undef isupper
#endif
#define isupper(c) CTYPE_CHECK(isupper, c)

#ifdef isxdigit
#undef isxdigit
#endif
#define isxdigit(c) CTYPE_CHECK(isxdigit, c)

#ifdef tolower
#undef tolower
#endif
#define tolower(c) CTYPE_CHECK(tolower, c)

#ifdef toupper
#undef toupper
#endif
#define toupper(c) CTYPE_CHECK(toupper, c)

#ifdef isascii
#undef isascii
#endif
#define isascii(c) CTYPE_CHECK(isascii, c)

#ifdef toascii
#undef toascii
#endif
#define toascii(c) CTYPE_CHECK(toascii, c)

#ifdef isblank
#undef isblank
#endif
#define isblank(c) CTYPE_CHECK(isblank, c)

#endif
