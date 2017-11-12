/* read_switch_multiword_via_readline.h -- get an answer from the user, with readline
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

/*
 * Same as read_switch_multiword but using readline(3) for input.
 */
#ifdef READLINE_SUPPORT
char **read_switch_multiword_via_readline(char *, struct swit *);
#endif /* READLINE_SUPPORT */
