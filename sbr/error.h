/* error.h -- main error handling routines
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

void inform(char *fmt, ...) CHECK_PRINTF(1, 2);
void advise(const char *, const char *, ...) CHECK_PRINTF(2, 3);
void adios(const char *, const char *, ...) CHECK_PRINTF(2, 3) NORETURN;
void die(const char *, ...) CHECK_PRINTF(1, 2) NORETURN;
void admonish(char *, char *, ...) CHECK_PRINTF(2, 3);
void advertise(const char *, char *, const char *, va_list) CHECK_PRINTF(3, 0);
