#! /bin/sh
#
# mhn.find.sh -- check if a particular command is available
#

if test -z "$2"; then
    echo "usage: mhn.find.sh search-path program" 1>&2
    exit 1
fi

# PATH to search for programs
SEARCHPATH=$1

# program to search for
PROGRAM=$2

PGM= oIFS="$IFS" IFS=":"
for A in $SEARCHPATH; do

    # skip the directories `.' and `..'
    if test "$A" = "." -o "$A" = ".."; then
	continue
    fi

    # if program was found in /usr/local/bin, then
    # just echo program name, else echo full pathname
    if test -f "$A/$PROGRAM"; then
	if test "$A" = "/usr/local/bin"; then
	    PGM="$PROGRAM"
	else
	    PGM="$A/$PROGRAM"
	fi

	echo "$PGM"
	exit 0
    fi
done
IFS="$oIFS"
