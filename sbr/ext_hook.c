/*	$Id$
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
    char	*vec[4];		/* argument vector for child process */

    static  int	did_message = 0;	/* set if we've already output a message */

    if ((hook = context_find(hook_name)) == (char *)0)
	return (OK);

    switch (pid = vfork()) {
    case -1:
	status = NOTOK;
	advise(NULL, "external database may be out-of-date.");
	break;

    case 0:
	vec[0] = r1bindex(hook, '/');
	vec[1] = message_file_name_1;
	vec[2] = message_file_name_2;
	vec[3] = (char *)0;
	execvp(hook, vec);
	_exit(-1);
	/* NOTREACHED */

    default:
	status = pidwait(pid, -1);
	break;
    }

    if (status != OK) {
	if (did_message == 0) {
	    if ((hook = context_find("msg-hook")) != (char *)0)
		advise(NULL, hook);
	    else
		advise(NULL, "external hook (%s) did not work properly.", hook);
	
	    did_message = 1;
	}

	return (NOTOK);
    }

    else
	return (OK);
}
