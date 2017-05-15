/* mhshowsbr.h -- display the contents of MIME messages.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */


/*
 * Display MIME message(s) on standard out.
 *
 * Arguments are:
 *
 * cts		- NULL terminated array of CT structures for messages
 *		  to display
 * concat	- If true, concatenate all MIME parts.  If false, show each
 *		  MIME part under a separate pager.
 * textonly	- If true, only display "text" MIME parts
 * inlineonly	- If true, only display MIME parts that are marked with
 *		  a disposition of "inline" (includes parts that lack a
 *		  Content-Disposition header).
 */
void show_all_messages(CT *cts, int concat, int textonly, int inlineonly);

/*
 * Display (or store) a single MIME part using the specified command
 *
 * Arguments are:
 *
 * ct		- The Content structure of the MIME part we wish to display
 * alternate	- Set this to true if this is one part of a MIME
 *		  multipart/alternative part.  Will suppress some errors and
 *		  will cause the function to return DONE instead of OK on
 *		  success.
 * cp		- The command string to execute.  Will be run through the
 *		  parser for %-escapes as described in mhshow(1).
 * cracked	- If set, chdir() to this directory before executing the
 *		  command in "cp".  Only used by mhstore(1).
 * fmt		- A series of mh-format(5) instructions to execute if the
 *		  command string indicates a marker is desired.  Can be NULL.
 *
 * Returns NOTOK if we could not display the part, DONE if alternate was
 * set and we could display the part, and OK if alternate was not set and
 * we could display the part.
 */
int show_content_aux(CT ct, int alternate, char *cp, char *cracked,
    struct format *fmt);

int convert_charset(CT ct, char *dest_charset, int *message_mods);

extern char *progsw;
extern int nomore;
extern char *formsw;
extern char *folder;
extern char *markerform;
extern char *headerform;
extern int headersw;
