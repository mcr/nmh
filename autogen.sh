#!/bin/sh
#
# Regenerate config.h.in, configure, etc.
# Necessary if building from CVS; not needed if
# building from a distributed tarball.

set -x
autoreconf -v -i
