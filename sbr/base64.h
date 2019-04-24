/* base64.h -- routines for converting to base64
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

int writeBase64aux(FILE *, FILE *, int);
int writeBase64(const unsigned char *, size_t, unsigned char *);
int writeBase64raw(const unsigned char *, size_t, unsigned char *);
int decodeBase64(const char *, unsigned char **, size_t *, int);
void hexify(const unsigned char *, size_t, char **);

/* Includes trailing NUL. */
#define BASE64SIZE(x) ((((x + 2) / 3) * 4) + 1)
