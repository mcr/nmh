#!/bin/sh
#
# mhn.defaults.sh -- create extra profile file for MIME handling
#
# $Id$
#
# USAGE: mhn.defaults.sh [ search-path [ search-prog ]]

# If a search path is passed to the script, we
# use that, else we use a default search path.
if [ -n "$1" ]; then
    SEARCHPATH=$1
else
    SEARCHPATH="$PATH:/usr/demo/SOUND"
fi

# If a search program is passed to the script, we
# use that, else we use a default search program.
if [ -n "$2" ]; then
    SEARCHPROG=$2
else
    SEARCHPROG="mhn.find.sh"
fi

# put output into a temporary file, so we
# can sort it before output.
TMP=/tmp/nmh_temp.$$
trap "rm -f $TMP" 0 1 2 3 13 15

echo "mhstore-store-text: %m%P.txt" >> $TMP
echo "mhstore-store-text/richtext: %m%P.rt" >> $TMP
echo "mhstore-store-video/mpeg: %m%P.mpg" >> $TMP
echo "mhstore-store-application/PostScript: %m%P.ps" >> $TMP

PGM="`$SEARCHPROG $SEARCHPATH xwud`"
if [ ! -z "$PGM" ]; then
    XWUD="$PGM" X11DIR="`echo $PGM | awk -F/ '{ for(i=2;i<NF;i++)printf "/%s", $i;}'`"/
else
    XWUD= X11DIR=
fi

PGM="`$SEARCHPROG $SEARCHPATH pbmtoxwd`"
if [ ! -z "$PGM" ]; then
    PBM="$PGM" PBMDIR="`echo $PGM | awk -F/ '{ for(i=2;i<NF;i++)printf "/%s", $i;}'`"/
else
    PBM= PBMDIR=
fi

PGM="`$SEARCHPROG $SEARCHPATH xv`"
if [ ! -z "$PGM" ]; then
    echo "mhshow-show-image: %p$PGM -geometry =-0+0 '%f'" >> $TMP
elif [ ! -z $"PBM" -a ! -z "$XWUD" ]; then
    echo "mhshow-show-image/gif: %p${PBMDIR}giftoppm | ${PBMDIR}ppmtopgm | ${PBMDIR}pgmtopbm | ${PBMDIR}pbmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    echo "mhshow-show-image/x-pbm: %p${PBMDIR}pbmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    echo "mhshow-show-image/x-pgm: %p${PBMDIR}pgmtopbm | ${PBMDIR}pbmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    echo "mhshow-show-image/x-ppm: %p${PBMDIR}ppmtopgm | ${PBMDIR}pgmtopbm | ${PBMDIR}pbmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    echo "mhshow-show-image/x-xwd: %p$XWUD -geometry =-0+0" >> $TMP

    PGM="`$SEARCHPROG $SEARCHPATH djpeg`"
    if [ ! -z "$PGM" ]; then
	echo "mhshow-show-image/jpeg: %p$PGM -Pg | ${PBMDIR}ppmtopgm | ${PBMDIR}pgmtopbm | ${PBMDIR}pbmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    fi
fi

if [ -f "/dev/audioIU" ]; then
    PGM="`$SEARCHPROG $SEARCHPATH recorder`"
    if [ ! -z "$PGM" ]; then
	echo "mhstore-store-audio/basic: %m%P.au" >> $TMP
        echo "mhbuild-compose-audio/basic: ${AUDIODIR}recorder '%f' -au -pause > /dev/tty" >> $TMP
        echo "mhshow-show-audio/basic: %p${AUDIODIR}splayer -au" >> $TMP
    fi
elif [ -f "/dev/audio" ]; then
    PGM="`$SEARCHPROG $SEARCHPATH raw2audio`"
    if [ ! -z "$PGM" ]; then
	AUDIODIR="`echo $PGM | awk -F/ '{ for(i=2;i<NF;i++)printf "/%s", $i;}'`"/
	echo "mhstore-store-audio/basic: | ${AUDIODIR}raw2audio -e ulaw -s 8000 -c 1 > %m%P.au" >> $TMP
        echo "mhstore-store-audio/x-next: %m%P.au" >> $TMP
	AUDIOTOOL="`$SEARCHPROG $SEARCHPATH audiotool`"
	if [ ! -z "$AUDIOTOOL" ]; then
	    echo "mhbuild-compose-audio/basic: $AUDIOTOOL '%f' && ${AUDIODIR}raw2audio -F < '%f'" >> $TMP
	else
	    echo "mhbuild-compose-audio/basic: trap \"exit 0\" 2 && ${AUDIODIR}record | ${AUDIODIR}raw2audio -F" >> $TMP
	fi
	echo "mhshow-show-audio/basic: %p${AUDIODIR}raw2audio 2>/dev/null | ${AUDIODIR}play" >> $TMP

	PGM="`$SEARCHPROG $SEARCHPATH adpcm_enc`"
	if [ ! -z "$PGM" ]; then
	    DIR="`echo $PGM | awk -F/ '{ for(i=2;i<NF;i++)printf "/%s", $i;}'`"/
	    if [ ! -z "$AUDIOTOOL" ]; then
		echo "mhbuild-compose-audio/x-next: $AUDIOTOOL '%f' && ${DIR}adpcm_enc < '%f'" >> $TMP
	    else
		echo "mhbuild-compose-audio/x-next: ${AUDIODIR}record | ${DIR}adpcm_enc" >> $TMP
	    fi
	    echo "mhshow-show-audio/x-next: %p${DIR}adpcm_dec | ${AUDIODIR}play" >> $TMP
	else
	    if [ ! -z "$AUDIOTOOL" ]; then
		echo "mhbuild-compose-audio/x-next: $AUDIOTOOL '%f'" >> $TMP
	    else
		echo "mhbuild-compose-audio/x-next: ${AUDIODIR}record" >> $TMP
	    fi
	    echo "mhshow-show-audio/x-next: %p${AUDIODIR}play" >> $TMP
	fi
    else
	echo "mhbuild-compose-audio/basic: cat < /dev/audio" >> $TMP
        echo "mhshow-show-audio/basic: %pcat > /dev/audio" >> $TMP
    fi
fi

PGM="`$SEARCHPROG $SEARCHPATH mpeg_play`"
if [ ! -z "$PGM" ]; then
	echo "mhshow-show-video/mpeg: %p$PGM '%f'" >> $TMP
fi

PGM="`$SEARCHPROG $SEARCHPATH lpr`"
if [ ! -z "$PGM" ]; then
	echo "mhshow-show-application/PostScript: %plpr -Pps" >> $TMP
else
    PGM="`$SEARCHPROG $SEARCHPATH lp`"
    if [ ! -z "$PGM" ]; then    
	echo "mhshow-show-application/PostScript: %plp -dps" >> $TMP
    fi
fi

PGM="`$SEARCHPROG $SEARCHPATH ivs_replay`"
if [ ! -z "$PGM" ]; then
	echo "mhshow-show-application/x-ivs: %p$PGM -o '%F'" >> $TMP
fi

echo "mhshow-suffix-text/html: .html" >> $TMP

# I'd like to check if netscape is available and use it preferentially to lynx,
# but only once I've added a new %-escape that makes more permanent temp files,
# so netscape -remote can be used (without -remote you get a complaint dialog
# that another netscape is already running and certain things can't be done).
PGM="`$SEARCHPROG $SEARCHPATH lynx`"
if [ ! -z "$PGM" ]; then
	echo "mhshow-show-text/html: %p$PGM '%F'" >> $TMP
fi

PGM="`$SEARCHPROG $SEARCHPATH richtext`"
if [ ! -z "$PGM" ]; then
	echo "mhshow-show-text/richtext: %p$PGM -p '%F'" >> $TMP
else
    PGM="`$SEARCHPROG $SEARCHPATH rt2raw`"
    if [ ! -z "$PGM" ]; then
	echo "mhshow-show-text/richtext: %p$PGM < '%f' | fmt -78 | more" >> $TMP
    fi
fi

# staroffice to read .doc files
PGM="`$SEARCHPROG $SEARCHPATH soffice`"
if [ ! -z "$PGM" ]; then
	echo "mhshow-show-application/msword: %psoffice '%F'" >> $TMP
	echo "mhshow-suffix-application/msword: .doc" >> $TMP
fi

PGM="`$SEARCHPROG $SEARCHPATH xterm`"
if [ ! -z "$PGM" ]; then
	echo "mhshow-charset-iso-8859-1: xterm -fn '-*-*-medium-r-normal-*-*-120-*-*-c-*-iso8859-*' -e %s" >> $TMP
fi

# output a sorted version of the file
sort < $TMP

exit 0

: not until we get a "safe" postscript environment...

PGM="`$SEARCHPROG $SEARCHPATH pageview`"
if [ "$DISPLAY" = "unix:0.0" -a ! -z "$PGM" ]; then
    echo "mhshow-show-application/PostScript: %p$PGM -" >> $TMP
else
    PGM="`$SEARCHPROG $SEARCHPATH gs`"
    if [ ! -z "$PGM" ]; then
	echo "mhshow-show-application/PostScript: %p$PGM -- '%F'" >> $TMP
	echo "mhshow-suffix-application/PostScript: .ps" >> $TMP
    fi
fi

: have to experiment more with this

PGM="`$SEARCHPROG $SEARCHPATH ivs_record`"
if [ ! -z "$PGM" ]; then
	echo "mhbuild-compose-application/x-ivs: $PGM -u localhost '%F'" >> $TMP
fi
