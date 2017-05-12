/* escape_addresses.c -- Escape address components, hopefully per RFC 5322.
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>

static void
escape_component (char *name, size_t namesize, char *chars);


void
escape_display_name (char *name, size_t namesize) {
  char *specials = "\"(),.:;<>@[\\]";
  escape_component (name, namesize, specials);
}


void
escape_local_part (char *name, size_t namesize) {
  /* wsp (whitespace) is horizontal tab or space, according to
     RFC 5234. */
  char *specials_less_dot_plus_wsp = "	 \"(),:;<>@[\\]";
  escape_component (name, namesize, specials_less_dot_plus_wsp);
}


/* Escape an address component, hopefully per RFC 5322.  Assumes
   one-byte characters.  The char array pointed to by the name
   argument is modified in place.  Its size is specified by the
   namesize argument.  The need_escape argument is a string of
   characters that require that name be escaped. */
void
escape_component (char *name, size_t namesize, char *chars_to_escape) {
    /* If name contains any chars_to_escape:
       1) enclose it in ""
       2) escape any embedded "
     */
    if (strpbrk(name, chars_to_escape)) {
        char *destp, *srcp;
        /* Maximum space requirement would be if each character had
           to be escaped, plus enclosing double quotes, plus NUL terminator.
           E.g., 2 characters, "", would require 7, "\"\""0, where that 0
           is '\0'. */
        char *tmp = mh_xmalloc (2*strlen(name) + 3);

        for (destp = tmp, srcp = name; *srcp; ++srcp) {
            if (srcp == name) {
                /* Insert initial double quote, if needed. */
                if (*srcp != '"') {
                    *destp++ = '"';
                }
            } else {
                /* Escape embedded, unescaped double quote. */
                if (*srcp == '"' && *(srcp+1) != '\0' && *(srcp-1) != '\\') {
                    *destp++ = '\\';
                }
            }

            *destp++ = *srcp;

            /* End of name. */
            if (*(srcp+1) == '\0') {
                /* Insert final double quote, if needed. */
                if (*srcp != '"') {
                    *destp++ = '"';
                }

                *destp++ = '\0';
            }
        }

        if (strcmp (tmp, "\"")) {
            size_t len = destp - tmp;
            assert ((ssize_t) strlen(tmp) + 1 == destp - tmp);
            strncpy (name, tmp, min(len, namesize));
        } else {
            /* Handle just " as special case here instead of above. */
            strncpy (name, "\"\\\"\"", namesize);
        }

        name[namesize - 1] = '\0';

        free (tmp);
    }
}
