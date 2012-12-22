
/*
 * m_msgdef.c -- some defines for sbr/m_getfld.c
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/*
 * disgusting hack for "inc" so it can know how many characters
 * were stuffed in the buffer on the last call (see comments
 * in uip/scansbr.c)
 */
int msg_count = 0;
