/* m_mktemp.h -- Construct a temporary file.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

char *m_mktemp(const char *pfx_in, int *fd_ret, FILE **fp_ret);
char *m_mktemp2(const char *dir_in, const char *pfx_in,
    int *fd_ret, FILE **fp_ret);
char *m_mktemps(const char *pfx_in, const char *suffix,
    int *fd_ret, FILE **fp_ret);
char *get_temp_dir(void);
void unregister_for_removal(int remove_files);
int m_unlink(const char *pathname);
void remove_registered_files_atexit(void);
void remove_registered_files(int sig);
