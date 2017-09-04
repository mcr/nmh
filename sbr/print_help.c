/* print_help.c -- print a help message, and possibly the
 *              -- profile/context entries for this command
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


void
print_help (char *str, struct swit *swp, int print_context)
{
    char *s;

    /* print Usage string */
    printf ("Usage: %s\n", str);

    /* print all the switches */
    puts("  switches are:");
    print_sw (ALL, swp, "-", stdout);

    /*
     * check if we should print any profile entries
     */
    if (print_context && (s = context_find (invo_name))) {
	printf ("\nProfile: %s\n", s);
    }

    /* and for further info */
    putchar('\n');
    print_intro (stdout, true);
    puts ("\nSee the BUGS section of the nmh(7) man page for more information.");
}


/*
 * The text below also appears in man/nmh.man.
 */

static const char nmh_intro1[] = \
"Send bug reports, questions, suggestions, and patches to\n"
"nmh-workers@nongnu.org.  That mailing list is relatively quiet, so user\n"
"questions are encouraged.  Users are also encouraged to subscribe, and\n"
"view the archives, at https://lists.gnu.org/mailman/listinfo/nmh-workers\n";

/* The text below is split so that string constant length doesn't
   exceed the C90 minimum maximum length of 509 characters. */
static const char nmh_intro2[] = \
"\n" \
"If problems are encountered with an nmh program, they should be\n"
"reported to the local maintainers of nmh, if any, or to the mailing\n"
"list noted above.  When doing this, the name of the program should be\n"
"reported, along with the version information for the program.\n";

static const char nmh_intro3[] = \
"\n"
"To find out what version of an nmh program is being run, invoke the\n"
"program with the -version switch.  This prints the version of nmh, the\n"
"host it was compiled on, and the date the program was linked.\n"
"\n"
"New releases and other information of potential interest are announced\n"
"at http://www.nongnu.org/nmh/ .\n";

void
print_intro (FILE *file, bool brief) {
    fputs (nmh_intro1, file);
    if (! brief) {
        fputs (nmh_intro2, file);
        fputs (nmh_intro3, file);
    }
}
