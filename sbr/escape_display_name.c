#include <sys/types.h>
#include <h/utils.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Escape a display name, hopefully per RFC 5322.
   The argument is assumed to be a pointer to a character array of
   one-byte characters with enough space to handle the additional
   characters. */
void
escape_display_name (char *name) {
    /* Quote and escape name that contains any specials, as necessary. */
    if (strpbrk("\"(),.:;<>@[\\]", name)) {
        size_t len = strlen(name);
        char *destp, *srcp;
        size_t destpos, srcpos;
        /* E.g., 2 characters, "", would require 7, "\"\""\0. */
	char *tmp = malloc (2*len+3);

        for (destp = tmp, srcp = name, destpos = 0, srcpos = 0;
             *srcp;
             ++destp, ++srcp, ++destpos, ++srcpos) {
            if (srcpos == 0) {
                /* Insert initial double quote, if needed. */
                if (*srcp != '"') {
                    *destp++ = '"';
                    ++destpos;
                }
            } else {
                /* Escape embedded, unescaped ". */
                if (*srcp == '"'  &&  srcpos < len - 1  &&  *(srcp-1) != '\\') {
                    *destp++ = '\\';
                    ++destpos;
                }
            }

            *destp = *srcp;

            /* End of name. */
            if (srcpos == len - 1) {
                /* Insert final double quote, if needed. */
                if (*srcp != '"') {
                    *++destp = '"';
                    ++destpos;
                }

                *++destp = '\0';
                ++destpos;
            }
        }

        if (strcmp (tmp, "\"")) {
            /* assert (strlen(tmp) + 1 == destpos); */
            strncpy (name, tmp, destpos);
        } else {
            /* Handle just " as special case here instead of above. */
            strcpy (name, "\"\\\"\"");
        }

        free (tmp);
    }
}
