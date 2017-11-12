/* m_getfld.h -- read/parse a message
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

m_getfld_state_t m_getfld_state_init(FILE *);
void m_getfld_state_reset(m_getfld_state_t *);
void m_getfld_track_filepos(m_getfld_state_t *, FILE *);
void m_getfld_track_filepos2(m_getfld_state_t *);
void m_getfld_state_destroy(m_getfld_state_t *);
int m_getfld(m_getfld_state_t *, char[NAMESZ], char *, int *, FILE *);
int m_getfld2(m_getfld_state_t *, char[NAMESZ], char *, int *);
void m_unknown(m_getfld_state_t *, FILE *);
void m_unknown2(m_getfld_state_t *);
