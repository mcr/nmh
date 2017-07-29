/* m_name.c -- return a message number as a string
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <limits.h>
#include <h/mh.h>
#include <h/utils.h>

#define STR(s) #s
#define SIZE(n) (sizeof STR(n)) /* Includes NUL. */

char *
m_name (int num)
{
    if (num <= 0) return "?";

    return m_strn(num, SIZE(INT_MAX));
}
