
/*
 * m_setjmp.h -- Wraps setjmp() and sigsetjmp(), to help prevent warnings
 *            -- about arguments and variables that might be clobbered by
 *            -- a setjmp call with gcc -Wclobbered.
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <setjmp.h>

int m_setjmp(jmp_buf);

int m_sigsetjmp(sigjmp_buf, int);
