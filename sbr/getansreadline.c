
/*
 * getansreadline.c -- get an answer from the user, with readline
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include <h/m_setjmp.h>
#include <signal.h>
#include <errno.h>

#ifdef READLINE_SUPPORT
#include <readline/readline.h>
#include <readline/history.h>

static struct swit *rl_cmds;

static char *nmh_command_generator(const char *, int);
static char **nmh_completion(const char *, int, int);
static void initialize_readline(void);

static char ansbuf[BUFSIZ];
#if 0
static sigjmp_buf sigenv;

/*
 * static prototypes
 */
static void intrser (int);


char **
getans (char *prompt, struct swit *ansp)
{
    int i;
    SIGNAL_HANDLER istat = NULL;
    char *cp, **cpp;

    if (!(sigsetjmp(sigenv, 1))) {
	istat = SIGNAL (SIGINT, intrser);
    } else {
	SIGNAL (SIGINT, istat);
	return NULL;
    }

    for (;;) {
	printf ("%s", prompt);
	fflush (stdout);
	cp = ansbuf;
	while ((i = getchar ()) != '\n') {
	    if (i == EOF) {
	    	/*
		 * If we get an EOF, return
		 */
		if (feof(stdin))
		    siglongjmp (sigenv, 1);

		/*
		 * For errors, if we get an EINTR that means that we got
		 * a signal and we should retry.  If we get another error,
		 * then just return.
		 */

		else if (ferror(stdin)) {
		    if (errno == EINTR) {
		    	clearerr(stdin);
			continue;
		    }
		    fprintf(stderr, "\nError %s during read\n",
		    	    strerror(errno));
		    siglongjmp (sigenv, 1);
		} else {
		    /*
		     * Just for completeness's sake ...
		     */

		    fprintf(stderr, "\nUnknown problem in getchar()\n");
		    siglongjmp (sigenv, 1);
		}
	    }
	    if (cp < &ansbuf[sizeof ansbuf - 1])
		*cp++ = i;
	}
	*cp = '\0';
	if (ansbuf[0] == '?' || cp == ansbuf) {
	    printf ("Options are:\n");
	    print_sw (ALL, ansp, "", stdout);
	    continue;
	}
	cpp = brkstring (ansbuf, " ", NULL);
	switch (smatch (*cpp, ansp)) {
	    case AMBIGSW: 
		ambigsw (*cpp, ansp);
		continue;
	    case UNKWNSW: 
		printf (" -%s unknown. Hit <CR> for help.\n", *cpp);
		continue;
	    default: 
		SIGNAL (SIGINT, istat);
		return cpp;
	}
    }
}


static void
intrser (int i)
{
    NMH_UNUSED (i);

    /*
     * should this be siglongjmp?
     */
    siglongjmp (sigenv, 1);
}
#endif

/*
 * getans, but with readline support
 */

char **
getans_via_readline(char *prompt, struct swit *ansp)
{
    char *ans, **cpp;

    initialize_readline();
    rl_cmds = ansp;

    for (;;) {
    	ans = readline(prompt);
	/*
	 * If we get an EOF, return
	 */

	if (ans == NULL)
	    return NULL;

	if (ans[0] == '?' || ans[0] == '\0') {
	    printf("Options are:\n");
	    print_sw(ALL, ansp, "", stdout);
	    free(ans);
	    continue;
	}
	add_history(ans);
	strncpy(ansbuf, ans, sizeof(ansbuf));
	ansbuf[sizeof(ansbuf) - 1] = '\0';
	cpp = brkstring(ansbuf, " ", NULL);
	switch (smatch(*cpp, ansp)) {
	    case AMBIGSW:
	    	ambigsw(*cpp, ansp);
		continue;
	    case UNKWNSW:
	    	printf(" -%s unknown. Hit <CR> for help.\n", *cpp);
		continue;
	    default:
	    	free(ans);
		return cpp;
	}
	free(ans);
    }
}

static void
initialize_readline(void)
{
    rl_readline_name = "Nmh";
    rl_attempted_completion_function = nmh_completion;
}

static char **
nmh_completion(const char *text, int start, int end)
{
    NMH_UNUSED (end);

    char **matches;

    matches = (char **) NULL;

    if (start == 0)
    	matches = rl_completion_matches(text, nmh_command_generator);

    return matches;
}

static char *
nmh_command_generator(const char *text, int state)
{
    static int list_index, len;
    char *name, *p;
    char buf[256];

    if (!state) {
    	list_index = 0;
	len = strlen(text);
    }

    while ((name = rl_cmds[list_index].sw)) {
    	list_index++;
	strncpy(buf, name, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	p = *brkstring(buf, " ", NULL);
	if (strncmp(p, text, len) == 0)
	return strdup(p);
    }

    return NULL;
}
#endif /* READLINE_SUPPORT */

