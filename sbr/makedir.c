
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
    pid_t pid;
    register char *cp;
    register char *c;
    char path[PATH_MAX];

    context_save();	/* save the context file */
    fflush(stdout);

    if (getuid () == geteuid ()) {
	    c = strncpy(path, dir, sizeof(path));     

	    while ((c = strchr((c + 1), '/')) != NULL) {	
		    *c = (char)0;
		    if (access(path, X_OK)) {
			    if (errno != ENOENT){
				    advise (dir, "unable to create directory");
				    return 0;
			    }			    
			    if (mkdir(path, 0775)) {
				    advise (dir, "unable to create directory");
				    return 0;
			    }
		    }
		    *c = '/';
	    }
 
	    if (mkdir (dir, 0755) == -1) {
		    advise (dir, "unable to create directory");
		    return 0;
	    }
    } else {
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
    }

    if (!(cp = context_find ("folder-protect")))
	cp = foldprot;
    chmod (dir, atooi (cp));
    return 1;
}
