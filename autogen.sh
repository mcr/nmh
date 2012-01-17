#!/bin/sh
#
# Regenerate config.h.in, configure, etc.
# Necessary if building from CVS; not needed if
# building from a distributed tarball.

set -e
aclocal -I m4
autoreconf
date > stamp-h.in
