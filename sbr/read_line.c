#include <h/mh.h>

const char *
read_line(void)
{
    char *cp;
    static char line[BUFSIZ];

    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
            return NULL;
    if ((cp = strchr(line, '\n')))
	*cp = 0;
    return line;
}

