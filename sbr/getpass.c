/*
 * Portions of this code are Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <h/mh.h>
#include <termios.h>

/* We don't use MAX_PASS here because the maximum password length on a remote
   POP daemon will have nothing to do with the length on our OS.  256 is
   arbitrary but hopefully big enough to accomodate everyone. */
#define MAX_PASSWORD_LEN 256

#ifndef TCSANOW
#define TCSANOW 0
#endif

char *
nmh_getpass(const char *prompt)
{
  struct termios oterm, term;
  int ch;
  char *p;
  FILE *fout, *fin;
  static char  buf[MAX_PASSWORD_LEN + 1];
  int istty = isatty(fileno(stdin));

  /* Find if stdin is connect to a terminal. If so, read directly from
   * the terminal, and turn off echo. Otherwise read from stdin.
   */

  if (!istty || !(fout = fin = fopen("/dev/tty", "w+"))) {
    fout = stderr;
    fin = stdin;
  }
  else /* Reading directly from terminal here */
    {
      (void)tcgetattr(fileno(fin), &oterm);
      term = oterm; /* Save original info */
      term.c_lflag &= ~ECHO;
      (void)fputs(prompt, fout);
      rewind(fout);			/* implied flush */
      (void)tcsetattr(fileno(fin), TCSANOW, &term);
    }

  for (p = buf; (ch = getc(fin)) != EOF &&
                 ch != '\n' &&
	         p < buf + MAX_PASSWORD_LEN;)
    *p++ = ch;
  *p = '\0';

  if (istty) {
    (void)tcsetattr(fileno(fin), TCSANOW, &oterm);
    rewind(fout);
    (void)fputc('\n', fout);
    (void)fclose(fin);
  }
  return buf;
}
