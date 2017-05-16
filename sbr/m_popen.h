/* m_popen.h -- redirect standard output to a popen(3)'d process.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 * Create a subprocess and redirect our standard output to it.
 *
 * Arguments are:
 *
 * name		- Name of process to create
 * savestdout	- If true, will save the current stdout file descriptor and
 *		  m_pclose() will close it at the appropriate time.
 */
void m_popen(char *name, int savestdout);

/*
 * Wait for the last process opened by m_popen().
 */
void m_pclose(void);
