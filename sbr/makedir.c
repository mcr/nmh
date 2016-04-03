
/*
 * makedir.c -- make a directory
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 * Modified to try recursive create.
 */

#include <h/mh.h>
#include <sys/file.h>

int
makedir (const char *dir)
{
    char            path[PATH_MAX];
    char*           folder_perms_ASCII;
    int             had_an_error = 0;
    mode_t          folder_perms, saved_umask;
    register char*  c;

    context_save();     /* save the context file */
    fflush(stdout);

    if (!(folder_perms_ASCII = context_find ("folder-protect")))
        folder_perms_ASCII = foldprot;  /* defaults to "700" */

    /* Because mh-profile.man documents "Folder-Protect:" as an octal constant,
       and we don't want to force the user to remember to include a leading
       zero, we call atooi(folder_perms_ASCII) here rather than
       strtoul(folder_perms_ASCII, NULL, 0).  Therefore, if anyone ever tries to
       specify a mode in say, hex, they'll get garbage.  (I guess nmh uses its
       atooi() function rather than calling strtoul() with a radix of 8 because
       some ancient platforms are missing that functionality. */
    folder_perms = atooi(folder_perms_ASCII);

    /* Folders have definite desired permissions that are set -- we don't want
       to interact with the umask.  Clear it temporarily. */
    saved_umask = umask(0);

    c = strncpy(path, dir, sizeof(path));

    while (!had_an_error && (c = strchr((c + 1), '/')) != NULL) {
        *c = (char)0;
        if (access(path, X_OK)) {
            if (errno != ENOENT){
                advise (dir, "unable to create directory");
                had_an_error = 1;
            }
            /* Create an outer directory. */
            if (mkdir(path, folder_perms)) {
                advise (dir, "unable to create directory");
                had_an_error = 1;
            }
        }
        *c = '/';
    }

    if (!had_an_error) {
        /* Create the innermost nested subdirectory of the path we're being
           asked to create. */
        if (mkdir (dir, folder_perms) == -1) {
            advise (dir, "unable to create directory");
            had_an_error = 1;
        }
    }

    umask(saved_umask);  /* put the user's umask back */

    if (had_an_error)
        return 0;  /* opposite of UNIX error return convention */
    else
        return 1;
}
