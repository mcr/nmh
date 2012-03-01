: run this script through /bin/sh
: $Id: pgpshow.sh,v 1.4 1996/02/09 01:32:45 jromine Exp $

CMD=$1 SHOW=more REFILE=N

X=$2.tmp Y=$2.out Z=$2
trap "rm -f $X $Y" 0 1 2 3 13 15

shift; shift
for A in $*; do
    A="`echo $A | tr '[A-Z]' '[a-z]'`"
    case "$A" in
	format=*)
	    if [ "$A" = "format=mime" ]; then
		SHOW="show -file"
		REFILE=T
	    fi
	    ;;

	x-*)
	    ;;

	*)  echo "usage: pgpshow -store/-show file"
	    exit 1
	    ;;	
    esac
done

case "$CMD" in
    -show)  rm -f $X $Y
	    if pgp $Z -o $X | tee $Y; then
		PGP_SIGNATURE=`grep "^Good signature from user " $Y | sed -e 's%^Good signature from user "\(.*\)".$%\1%'`
		export PGP_SIGNATURE
		$SHOW $X
	    else
		exit 1
	    fi
	    ;;

    -store) cat > $X
	    pgp $X -o $Z
	    if [ "$REFILE" = "T" ]; then
		refile -file $Z +inbox
	    fi
	    ;;

    *)	    echo "usage: pgpshow -store/-show file"
	    exit 1
	    ;;
esac

exit 0
