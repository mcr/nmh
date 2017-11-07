/* readconfig.h -- base routine to read nmh configuration files
 *              -- such as nmh profile, context file, or mhn.defaults.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

void readconfig(struct node **, FILE *, const char *, int);
void add_profile_entry(const char *, const char *);
