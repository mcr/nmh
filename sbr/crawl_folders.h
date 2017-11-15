/* crawl_folders.h -- crawl folder hierarchy
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

#define CRAWL_NUMFOLDERS 100

/* Callbacks return true crawl_folders should crawl the children of `folder'.
 * Callbacks need not duplicate folder, as crawl_folders does not free it. */
typedef bool (crawl_callback_t)(char *, void *);

/* Crawl the folder hierarchy rooted at the relative path `dir'.  For each
 * folder, pass `callback' the folder name (as a path relative to the current
 * directory) and `baton'; the callback may direct crawl_folders not to crawl
 * its children; see above. */
void crawl_folders(char *, crawl_callback_t *, void *);
