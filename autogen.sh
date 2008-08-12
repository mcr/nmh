#!/bin/sh
#
# Regenerate config.h.in, configure, etc.
# Necessary if building from CVS; not needed if
# building from a distributed tarball.

set -e
autoheader
autoconf
date > stamp-h.in
