/* m_mktemp.h -- Construct a temporary file.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

void remove_registered_files_atexit(void);
void remove_registered_files(int sig);
