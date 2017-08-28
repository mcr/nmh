/* terminal.h -- termcap support
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

int sc_width(void);
int sc_length(void);
void nmh_clear_screen(void);
int SOprintf(char *fmt, ...) CHECK_PRINTF(1, 2);
char *get_term_stringcap(char *capability);
char *get_term_stringparm(char *capability, long arg1, long arg2);
int get_term_numcap(char *capability);
