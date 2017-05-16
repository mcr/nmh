/* unquote.h -- Handle quote removal and quoted-pair strings on
 * RFC 2822-5322 atoms.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/* Remove quotes and quoted-pair sequences from RFC-5322 atoms.
 *
 * Currently the actual algorithm is simpler than it technically should
 * be: any quotes are simply eaten, unless they're preceded by the escape
 * character (\).  This seems to be sufficient for our needs for now.
 *
 * Arguments:
 *
 * input	- The input string
 * output	- The output string; is assumed to have at least as much
 *		  room as the input string.  At worst the output string
 *		  will be the same size as the input string; it might be
 *		  smaller. */
void unquote_string(const char *input, char *output);
