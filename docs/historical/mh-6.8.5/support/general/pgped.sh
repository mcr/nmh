: run this script through /bin/sh

if [ "$1" = -sign ]; then
    FLAGS=sat
    TEXT=T
    shift
else
    FLAGS=esat
    TEXT=F
fi

if [ "$#" != 1 ]; then
    echo "usage: What now? ed pgped" 1>&2
    exit 1
fi
DRAFT=$1

trap "rm -f $DRAFT.1 $DRAFT.2 $DRAFT.2.asc" 0 1 2 3 13 15

USERS="`stclsh -nointerface -messaging -messagebody $DRAFT -file @LIB/pgped.tcl $DRAFT.1 $DRAFT.2 $TEXT`"

if [ ! -s $DRAFT.1 -o ! -s $DRAFT.2 ]; then
    exit 1
fi

if pgp -$FLAGS $DRAFT.2 $USERS; then
    mv $DRAFT `dirname $DRAFT`/,`basename $DRAFT`

    cat $DRAFT.1 $DRAFT.2.asc > $DRAFT

    exit 0
fi

exit 1
