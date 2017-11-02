/* charstring.c -- dynamically-sized char array that can report size
 *               in both characters and bytes
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "h/utils.h"

#ifdef MULTIBYTE_SUPPORT
#  define NMH_MAX_CHARWIDTH MB_CUR_MAX
#else
#  define NMH_MAX_CHARWIDTH 1
#endif

#define CHARSTRING_DEFAULT_SIZE 64

struct charstring {
    char *buffer;  /* the char array, not always null-terminated */
    size_t max;    /* current size of the char array, in bytes */
    char *cur;     /* size in bytes = cur - buffer, without trailing null */
    size_t chars;  /* size in characters */
};


static void
charstring_reserve (charstring_t s, size_t need)
{
    const size_t cur = s->cur - s->buffer;

    while (need >= s->max - cur) {
        /* Insufficient capacity, so double it. */
        s->buffer = mh_xrealloc (s->buffer, s->max *= 2);
        s->cur = s->buffer + cur;
    }
}

/*
 * max is in characters
 */
charstring_t
charstring_create (size_t max)
{
    charstring_t s;

    NEW(s);
    s->max = NMH_MAX_CHARWIDTH * (max ? max : CHARSTRING_DEFAULT_SIZE);
    s->cur = s->buffer = mh_xmalloc (s->max);
    s->chars = 0;

    return s;
}

charstring_t
charstring_copy (const charstring_t src)
{
    const size_t num = src->cur - src->buffer;
    charstring_t s;

    NEW(s);
    s->max = src->max;
    s->buffer = mh_xmalloc (s->max);
    memcpy (s->buffer, src->buffer, num);
    s->cur = s->buffer + num;
    s->chars = src->chars;

    return s;
}

/*
 * OK to call charstring_free with a NULL argument.
 */
void
charstring_free (charstring_t s)
{
    if (s) {
        free (s->buffer);
        free (s);
    }
}

void
charstring_push_back (charstring_t s, const char c)
{
    charstring_reserve (s, s->cur - s->buffer + 1);
    *s->cur++ = c;
    ++s->chars;
}

/*
 * num is the number of bytes in c, width is the display width
 * occupied by the character(s).
 */
void
charstring_push_back_chars (charstring_t s, const char c[], size_t num,
                            size_t width)
{
    size_t i;

    charstring_reserve (s, s->cur - s->buffer + num);
    for (i = 0; i < num; ++i) { *s->cur++ = *c++; }
    s->chars += width;
}

void
charstring_append (charstring_t dest, const charstring_t src)
{
    const size_t num = src->cur - src->buffer;

    if (num > 0) {
        charstring_reserve (dest, dest->cur - dest->buffer + num);
        memcpy (dest->cur, src->buffer, num);
        dest->cur += num;
        dest->chars += src->chars;
    }
}

void
charstring_append_cstring (charstring_t dest, const char src[])
{
    const size_t num = strlen (src);

    if (num > 0) {
        charstring_reserve (dest, dest->cur - dest->buffer + num);
        memcpy (dest->cur, src, num);  /* Exclude src's trailing newline. */
        dest->cur += num;
        dest->chars += num;
    }
}

void
charstring_clear (charstring_t s)
{
    s->cur = s->buffer;
    s->chars = 0;
}

/*
 * Don't store return value of charstring_buffer() and use later after
 * intervening push_back's; use charstring_buffer_copy() instead.
 */
const char *
charstring_buffer (const charstring_t s)
{
    charstring_reserve (s, s->cur - s->buffer + 1);

    /* This is the only place that we null-terminate the buffer. */
    *s->cur = '\0';
    /* Don't increment cur so that more can be appended later, and so
       that charstring_bytes() behaves as strlen() by not counting the
       null. */

    return s->buffer;
}

char *
charstring_buffer_copy (const charstring_t s)
{
    char *copy = mh_xmalloc (s->cur - s->buffer + 1);

    /* Use charstring_buffer() to null terminate the buffer. */
    memcpy (copy, charstring_buffer (s), s->cur - s->buffer + 1);

    return copy;
}

size_t
charstring_bytes (const charstring_t s)
{
    return s->cur - s->buffer;
}

size_t
charstring_chars (const charstring_t s)
{
    return s->chars;
}

int
charstring_last_char_len (const charstring_t s)
{
    int len = 0;
#ifdef MULTIBYTE_SUPPORT
    const char *sp = charstring_buffer (s);
    size_t remaining = charstring_bytes (s);

    if (mbtowc (NULL, NULL, 0)) {} /* reset shift state */

    while (*sp  &&  remaining > 0) {
        wchar_t wide_char;

        len = mbtowc (&wide_char, sp, (size_t) MB_CUR_MAX < remaining
                                          ? (size_t) MB_CUR_MAX
                                          : remaining);
        sp += max(len, 1);
        remaining -= max(len, 1);
    }
#else  /* ! MULTIBYTE_SUPPORT */
    if (charstring_bytes (s) > 0) { len = 1; }
#endif /* ! MULTIBYTE_SUPPORT */

    return len;
}
