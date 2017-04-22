/*
 * crawl_folders.c -- crawl folder hierarchy
 *
 * This code is Copyright (c) 2008, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/crawl_folders.h>
#include <h/utils.h>

struct crawl_context {
    int max;			/* how many folders we currently can hold in
				 * the array `folders', increased by
				 * CRAWL_NUMFOLDERS at a time */
    char **folders;		/* the array of folders */
    int start;
    int foldp;
};

/*
 * Add the folder name into the
 * list in a sorted fashion.
 */

static void
add_folder (char *fold, struct crawl_context *crawl)
{
    int i, j;

    /* if necessary, reallocate the space for folder names */
    if (crawl->foldp >= crawl->max) {
	crawl->max += CRAWL_NUMFOLDERS;
	crawl->folders = mh_xrealloc (crawl->folders,
				      crawl->max * sizeof(char *));
    }

    for (i = crawl->start; i < crawl->foldp; i++)
	if (strcmp (fold, crawl->folders[i]) < 0) {
	    for (j = crawl->foldp - 1; j >= i; j--)
		crawl->folders[j + 1] = crawl->folders[j];
	    crawl->foldp++;
	    crawl->folders[i] = fold;
	    return;
	}

    crawl->folders[crawl->foldp++] = fold;
}

static void
add_children (char *name, struct crawl_context *crawl)
{
    char *prefix, *child;
    struct stat st;
    struct dirent *dp;
    DIR * dd;
    int child_is_folder;

    if (!(dd = opendir (name))) {
	admonish (name, "unable to read directory ");
	return;
    }

    if (strcmp (name, ".") == 0) {
	prefix = mh_xstrdup("");
    } else {
	prefix = concat (name, "/", (void *)NULL);
    }

    while ((dp = readdir (dd))) {
	/* If the system supports it, try to skip processing of children we
	 * know are not directories or symlinks. */
	child_is_folder = -1;
#if defined(HAVE_STRUCT_DIRENT_D_TYPE)
	if (dp->d_type == DT_DIR) {
	    child_is_folder = 1;
	} else if (dp->d_type != DT_LNK && dp->d_type != DT_UNKNOWN) {
	    continue;
	}
#endif
	if (!strcmp (dp->d_name, ".") || !strcmp (dp->d_name, "..")) {
	    continue;
	}
	child = concat (prefix, dp->d_name, (void *)NULL);
	/* If we have no d_type or d_type is DT_LNK or DT_UNKNOWN, stat the
	 * child to see what it is. */
	if (child_is_folder == -1) {
	    child_is_folder = (stat (child, &st) != -1 && S_ISDIR(st.st_mode));
	}
	if (child_is_folder) {
	    /* add_folder saves child in the list, don't free it */
	    add_folder (child, crawl);
	} else {
	    free (child);
	}
    }

    closedir (dd);
    free(prefix);
}

static void
crawl_folders_body (struct crawl_context *crawl,
		    char *dir, crawl_callback_t *callback, void *baton)
{
    int i;
    int os = crawl->start;
    int of = crawl->foldp;

    crawl->start = crawl->foldp;

    add_children (dir, crawl);

    for (i = crawl->start; i < crawl->foldp; i++) {
	char *fold = crawl->folders[i];
	int crawl_children = 1;

	if (callback != NULL) {
	    crawl_children = callback (fold, baton);
	}

	if (crawl_children) {
	    crawl_folders_body (crawl, fold, callback, baton);
	}
    }

    crawl->start = os;
    crawl->foldp = of;
}

void
crawl_folders (char *dir, crawl_callback_t *callback, void *baton)
{
    struct crawl_context *crawl;
    NEW(crawl);
    crawl->max = CRAWL_NUMFOLDERS;
    crawl->start = crawl->foldp = 0;
    crawl->folders = mh_xmalloc (crawl->max * sizeof(*crawl->folders));

    crawl_folders_body (crawl, dir, callback, baton);

    /* Note that we "leak" the folder names, on the assumption that the caller
     * is using them. */
    free (crawl->folders);
    free (crawl);
}
