
/*
 * makedir.c -- make a directory
 *
 * $Id$
 */

/*
 * Modified to try recursive create.
 */

#include <h/mh.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/file.h>

extern int errno;
	
int
makedir (char *dir)
{
    char            path[PATH_MAX];
    char*           folder_perms_ASCII;
    int             had_an_error = 0;
    mode_t          folder_perms, saved_umask;
    pid_t           pid;
    register char*  c;

    context_save();	/* save the context file */
    fflush(stdout);

    if (!(folder_perms_ASCII = context_find ("folder-protect")))
	folder_perms_ASCII = foldprot;  /* defaults to "0700" */
    
    /* Call strtoul() with radix=0, which means it'll determine the base by
       looking at the first digit.  That way it'll know "0700" is an octal
       constant (and if someone gets wacky and changes the representation to hex
       it'll still work). */
    folder_perms = strtoul(folder_perms_ASCII, NULL, 0);

    /* Folders have definite desired permissions that are set -- we don't want
       to interact with the umask.  Clear it temporarily. */
    saved_umask = umask(0);

    if (getuid () == geteuid ()) {
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
    }
    else {
	/* Ummm, why do we want to avoid creating directories with the effective
	   user ID?  None of the nmh tools are installed such that the effective
	   should be different from the real, and if some parent process made
	   the two be different, I don't see why it should be our job to enforce
	   the real UID.  Also, why the heck do we call the mkdir executable
	   rather than the library function in this case??  If we do want to
	   call the mkdir executable, we should at least be giving it -p (and
	   change the single chmod() call below) so it can successfully create
	   nested directories like the above code can.

	   -- Dan Harkless <dan-nmh@dilvish.speed.net> */
	switch (pid = vfork()) {
	    case -1: 
		advise ("fork", "unable to");
		return 0;
		
	    case 0: 
		setgid (getgid ());
		setuid (getuid ());
		
		execl ("/bin/mkdir", "mkdir", dir, NULL);
		execl ("/usr/bin/mkdir", "mkdir", dir, NULL);
		fprintf (stderr, "unable to exec ");
		perror ("mkdir");
		_exit (-1);
		
	    default: 
		if (pidXwait(pid, "mkdir"))
		    return 0;
		break;
	}

	chmod (dir, folder_perms);
    }

    umask(saved_umask);  /* put the user's umask back */

    if (had_an_error)
	return 0;  /* opposite of UNIX error return convention */
    else
	return 1;
}
