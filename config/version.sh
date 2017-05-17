#! /bin/sh
#
# version.sh -- script to create version string(s) for nmh.
# You need to pass the script the version number to use.

set -e

version=${1?}
host=`uname -n`
if test -d "$srcdir/.git"; then
    git=" `git -C $srcdir describe --long --dirty`"
else
    git=
fi
date="`TZ=GMT0 date +'%Y-%m-%d %T'` +0000"

cat <<E
char *version_str = "nmh-$version$git built $date on $host";
char *version_num = "nmh-$version";
char *user_agent = "nmh/$version";
E
