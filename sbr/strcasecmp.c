
/*
 * strcasecmp.c -- compare strings, ignoring case
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/*
 * Our version of strcasecmp has to deal with NULL strings.
 * Once that is fixed in the rest of the code, we can use the
 * native version, instead of this one.
 */

int
mh_strcasecmp (const char *s1, const char *s2) 
{
    const unsigned char *us1, *us2;

    us1 = (const unsigned char *) s1,
    us2 = (const unsigned char *) s2;

    if (!us1)
	us1 = (const unsigned char *) "";
    if (!us2)
	us2 = (const unsigned char *) "";
 
    while (tolower(*us1) == tolower(*us2++)) 
	if (*us1++ == '\0')
	    return (0);
    return (tolower(*us1) - tolower(*--us2));
}
 

int
mh_strncasecmp (const char *s1, const char *s2, size_t n)
{
    const unsigned char *us1, *us2;

    if (n != 0) { 
	us1 = (const unsigned char *) s1,
	us2 = (const unsigned char *) s2;

	do {  
	    if (tolower(*us1) != tolower(*us2++))
		return (tolower(*us1) - tolower(*--us2));
	    if (*us1++ == '\0')
		break;  
	} while (--n != 0);
    } 
    return (0);
}
