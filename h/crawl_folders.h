/*
 * crawl_folders.h -- crawl folder hierarchy
 */

#define CRAWL_NUMFOLDERS 100

/* Callbacks return TRUE crawl_folders should crawl the children of `folder'.
 * Callbacks need not duplicate folder, as crawl_folders does not free it. */
typedef boolean (crawl_callback_t)(char *folder, void *baton);

/* Crawl the folder hierarchy rooted at the relative path `dir'.  For each
 * folder, pass `callback' the folder name (as a path relative to the current
 * directory) and `baton'; the callback may direct crawl_folders not to crawl
 * its children; see above. */
void crawl_folders (char *dir, crawl_callback_t *callback, void *baton);
