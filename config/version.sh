#!/bin/sh
#
# version.sh -- script to create version string(s) for nmh.
#
# You need to pass the script the version number to use.
#

if [ -z "$1" ]; then
    echo "usage: version.sh VERSION" 1>&2
    exit 1
fi

VERSION=$1
OFS="$IFS"
IFS=:
HOSTNAME=unknown

# Find out the name of the host we are compiling on
for prog in uname hostname
do
    for dir in $PATH
    do
	if [ ! -f $dir/$prog ]; then
	    continue
	fi
	case $prog in
	    uname)	HOSTNAME=`$prog -n`
			;;
	    hostname)	HOSTNAME=`$prog`
			;;
	esac
	break
    done
    if [ X"$HOSTNAME" != X  -a  X"$HOSTNAME" != Xunknown ]; then
	break
    fi
done

IFS=" "

if [ -d "${srcdir}/.git" ] ; then
    branch=`(git branch | grep '^\*' | tr -d '* ') || true`
fi
if [ "${branch}" -a "${branch}" != "master" ] ; then
    echo "char *version_str = \"nmh-$VERSION [branch ${branch}] [compiled on $HOSTNAME at `date`]\";"
else
    echo "char *version_str = \"nmh-$VERSION [compiled on $HOSTNAME at `date`]\";"
fi
echo "char *version_num = \"nmh-$VERSION\";"
