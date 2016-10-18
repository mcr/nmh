#include <h/mh.h>
#include <h/utils.h>

const char *
read_line(void)
{
    static char line[BUFSIZ];

    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
            return NULL;
    TrimSuffixC(line, '\n');

    return line; /* May not be a complete line. */
}
