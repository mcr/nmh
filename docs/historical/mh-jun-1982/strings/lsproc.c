#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/*
 * This is standard ls, but "folder -all -short" calls it with
 * a -x switch meaning "list only directories".
 *
 * Also, its nice to have columnated output rather than a simple
 * list... (personal prejudice).
 */

char    *lsproc =       "/usr/ucb/ls";
