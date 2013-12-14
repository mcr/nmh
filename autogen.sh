#!/bin/sh
#
# Regenerate config.h.in, configure, etc.
# Necessary if building from CVS; not needed if
# building from a distributed tarball.

if [ `uname` = "OpenBSD" ] ; then
	export AUTOCONF_VERSION=2.69
	export AUTOMAKE_VERSION=1.13
fi

set -x

autoreconf -v -i
