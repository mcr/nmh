/* getcpy.h -- copy a string in managed memory
 *
 * THIS IS OBSOLETE.  NEED TO REPLACE ALL OCCURRENCES
 * OF GETCPY WITH STRDUP.  BUT THIS WILL REQUIRE
 * CHANGING PARTS OF THE CODE TO DEAL WITH NULL VALUES.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

/* Return malloc'd copy of str, or of "" if NULL, exit on failure. */
char *getcpy(const char *str);
