/* geteditor.c -- Determine the default editor to use
 *
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "geteditor.h"
#include "context_find.h"
#include "h/utils.h"

static char *default_editor = NULL;

char *
get_default_editor(void)
{
    char *str;

    if (default_editor)
	return default_editor;

    if (!(str = context_find("editor")) && !(str = getenv("VISUAL")) &&
	!(str = getenv("EDITOR"))) {
	str = "prompter";
    }

    return default_editor = str;
}
