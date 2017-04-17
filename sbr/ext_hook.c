/*
 *
 *	Run a program that hooks into some other system.  The first argument is
 *	name of the hook to use, the second is the full path name of a mail message.
 *	The third argument is also the full path name of a mail message, or a NULL
 *	pointer if it isn't needed.  Look in the context for an error message if
 *	something goes wrong; there is a built-in message in case one isn't specified.
 *	Only produce the error message once.
 */

#include <h/mh.h>

int
ext_hook(char *hook_name, char *message_file_name_1, char *message_file_name_2)
{
    char	*hook;			/* hook program from context */
    pid_t	pid;			/* ID of child process */
    int		status;			/* exit or other child process status */
    char	**vec;			/* argument vector for child process */
    int		vecp;			/* Vector index */
    char 	*program;		/* Name of program to execute */

    static  int	did_message = 0;	/* set if we've already output a message */

    if ((hook = context_find(hook_name)) == NULL)
	return (OK);

    switch (pid = fork()) {
    case -1:
	status = NOTOK;
	inform("external database may be out-of-date.");
	break;

    case 0:
        vec = argsplit(hook, &program, &vecp);
	vec[vecp++] = message_file_name_1;
	vec[vecp++] = message_file_name_2;
	vec[vecp++] = NULL;
	execvp(program, vec);
	advise(program, "Unable to execute");
	_exit(-1);
	/* NOTREACHED */

    default:
	status = pidwait(pid, -1);
	break;
    }

    if (status != OK) {
	if (did_message == 0) {
	    char *msghook;
	    if ((msghook = context_find("msg-hook")) != NULL)
		inform(msghook);
	    else {
	    	char errbuf[BUFSIZ];
		snprintf(errbuf, sizeof(errbuf), "external hook \"%s\"", hook);
		pidstatus(status, stderr, errbuf);
	    }
	    did_message = 1;
	}

	return (NOTOK);
    }

    else
	return (OK);
}
