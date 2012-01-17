
/*
 * m_vfork.c -- Wraps vfork(), to help prevent warnings about arguments
 *           -- and variables that might be clobbered by a vfork call
 *           -- with gcc -Wclobbered.
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

pid_t
m_vfork() {
  return vfork();
}
