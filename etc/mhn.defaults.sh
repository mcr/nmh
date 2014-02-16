#!/bin/sh
#
# mhn.defaults.sh -- create extra profile file for MIME handling
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


if [ ! -z "`$SEARCHPROG $SEARCHPATH w3m`" ]; then
  echo "mhfixmsg-format-text/html: w3m -dump -T text/html -O utf-8 '%F'" >> $TMP
elif [ ! -z "`$SEARCHPROG $SEARCHPATH lynx`" ]; then
  #### lynx indents with 3 spaces, remove them and any trailing spaces.
  echo "mhfixmsg-format-text/html: lynx -child -dump -force_html '%F' | \
expand | sed -e 's/^   //' -e 's/  *$//'" >> $TMP
elif [ ! -z "`$SEARCHPROG $SEARCHPATH elinks`" ]; then
  echo "mhfixmsg-format-text/html: elinks -dump -force-html -no-numbering \
-eval 'set document.browse.margin_width = 0' '%F'" >> $TMP
fi


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

PGM="`$SEARCHPROG $SEARCHPATH pnmtoxwd`"
if [ ! -z "$PGM" ]; then
    NETPBM="$PGM" NETPBMDIR="`echo $PGM | awk -F/ '{ for(i=2;i<NF;i++)printf "/%s", $i;}'`"/
else
    NETPBM= NETPBMDIR=
fi

PGM="`$SEARCHPROG $SEARCHPATH xv`"
if [ ! -z "$PGM" ]; then
    echo "mhshow-show-image: %p$PGM -geometry =-0+0 '%f'" >> $TMP
elif [ ! -z $"NETPBM" -a ! -z "$XWUD" ]; then
    echo "mhshow-show-image/gif: %p${NETPBMDIR}giftopnm | ${NETPBMDIR}ppmtopgm | ${NETPBMDIR}pgmtopbm | ${NETPBMDIR}pnmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    echo "mhshow-show-image/x-pnm: %p${NETPBMDIR}pnmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    echo "mhshow-show-image/x-pgm: %p${NETPBMDIR}pgmtopbm | ${NETPBMDIR}pnmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    echo "mhshow-show-image/x-ppm: %p${NETPBMDIR}ppmtopgm | ${NETPBMDIR}pgmtopbm | ${NETPBMDIR}pnmtoxwd | $XWUD -geometry =-0+0" >> $TMP
    echo "mhshow-show-image/x-xwd: %p$XWUD -geometry =-0+0" >> $TMP

    PGM="`$SEARCHPROG $SEARCHPATH djpeg`"
    if [ ! -z "$PGM" ]; then
	echo "mhshow-show-image/jpeg: %p$PGM -Pg | ${NETPBMDIR}ppmtopgm | ${NETPBMDIR}pgmtopbm | ${NETPBMDIR}pnmtoxwd | $XWUD -geometry =-0+0" >> $TMP
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

# The application/vnd.openxmlformats-officedocument.wordprocessingml.document
# through application/onenote associations are from
# http://technet.microsoft.com/en-us/library/cc179224.aspx

cat <<EOF >> ${TMP}
mhshow-suffix-application/msword: .doc
mhshow-suffix-application/ogg: .ogg
mhshow-suffix-application/pdf: .pdf
mhshow-suffix-application/postscript: .ps
mhshow-suffix-application/rtf: .rtf
mhshow-suffix-application/vnd.ms-excel: .xla
mhshow-suffix-application/vnd.ms-excel: .xlc
mhshow-suffix-application/vnd.ms-excel: .xld
mhshow-suffix-application/vnd.ms-excel: .xll
mhshow-suffix-application/vnd.ms-excel: .xlm
mhshow-suffix-application/vnd.ms-excel: .xls
mhshow-suffix-application/vnd.ms-excel: .xlt
mhshow-suffix-application/vnd.ms-excel: .xlw
mhshow-suffix-application/vnd.ms-powerpoint: .pot
mhshow-suffix-application/vnd.ms-powerpoint: .pps
mhshow-suffix-application/vnd.ms-powerpoint: .ppt
mhshow-suffix-application/vnd.ms-powerpoint: .ppz
mhshow-suffix-application/vnd.openxmlformats-officedocument.wordprocessingml.document: .docx
mhshow-suffix-application/vnd.ms-word.document.macroEnabled.12: .docm
mhshow-suffix-application/vnd.openxmlformats-officedocument.wordprocessingml.template: .dotx
mhshow-suffix-application/vnd.ms-word.template.macroEnabled.12: .dotm
mhshow-suffix-application/vnd.openxmlformats-officedocument.spreadsheetml.sheet: .xlsx
mhshow-suffix-application/vnd.ms-excel.sheet.macroEnabled.12: .xlsm
mhshow-suffix-application/vnd.openxmlformats-officedocument.spreadsheetml.template: .xltx
mhshow-suffix-application/vnd.ms-excel.template.macroEnabled.12: .xltm
mhshow-suffix-application/vnd.ms-excel.sheet.binary.macroEnabled.12: .xlsb
mhshow-suffix-application/vnd.ms-excel.addin.macroEnabled.12: .xlam
mhshow-suffix-application/vnd.openxmlformats-officedocument.presentationml.presentation: .pptx
mhshow-suffix-application/vnd.ms-powerpoint.presentation.macroEnabled.12: .pptm
mhshow-suffix-application/vnd.openxmlformats-officedocument.presentationml.slideshow: .ppsx
mhshow-suffix-application/vnd.ms-powerpoint.slideshow.macroEnabled.12: .ppsm
mhshow-suffix-application/vnd.openxmlformats-officedocument.presentationml.template: .potx
mhshow-suffix-application/vnd.ms-powerpoint.template.macroEnabled.12: .potm
mhshow-suffix-application/vnd.ms-powerpoint.addin.macroEnabled.12: .ppam
mhshow-suffix-application/vnd.openxmlformats-officedocument.presentationml.slide: .sldx
mhshow-suffix-application/vnd.ms-powerpoint.slide.macroEnabled.12: .sldm
mhshow-suffix-application/onenote: .onetoc
mhshow-suffix-application/onenote: .onetoc2
mhshow-suffix-application/onenote: .onetmp
mhshow-suffix-application/onenote: .onepkg
mhshow-suffix-application/x-bzip2: .bz2
mhshow-suffix-application/x-cpio: .cpio
mhshow-suffix-application/x-dvi: .dvi
mhshow-suffix-application/x-gzip: .gz
mhshow-suffix-application/x-java-archive: .jar
mhshow-suffix-application/x-javascript: .js
mhshow-suffix-application/x-latex: .latex
mhshow-suffix-application/x-sh: .sh
mhshow-suffix-application/x-tar: .tar
mhshow-suffix-application/x-texinfo: .texinfo
mhshow-suffix-application/x-tex: .tex
mhshow-suffix-application/x-troff-man: .man
mhshow-suffix-application/x-troff-me: .me
mhshow-suffix-application/x-troff-ms: .ms
mhshow-suffix-application/x-troff: .t
mhshow-suffix-application/zip: .zip
mhshow-suffix-audio/basic: .au
mhshow-suffix-audio/midi: .midi
mhshow-suffix-audio/mpeg: .mp3
mhshow-suffix-audio/mpeg: .mpg
mhshow-suffix-audio/x-ms-wma: .wma
mhshow-suffix-audio/x-wav: .wav
mhshow-suffix-image/gif: .gif
mhshow-suffix-image/jpeg: .jpeg
mhshow-suffix-image/jpeg: .jpg
mhshow-suffix-image/png: .png
mhshow-suffix-image/tiff: .tif
mhshow-suffix-image/tiff: .tiff
mhshow-suffix-text/calendar: .ics
mhshow-suffix-text/css: .css
mhshow-suffix-text/html: .html
mhshow-suffix-text/rtf: .rtf
mhshow-suffix-text/sgml: .sgml
mhshow-suffix-text/xml: .xml
mhshow-suffix-video/mpeg: .mpeg
mhshow-suffix-video/mpeg: .mpg
mhshow-suffix-video/quicktime: .moov
mhshow-suffix-video/quicktime: .mov
mhshow-suffix-video/quicktime: .qt
mhshow-suffix-video/quicktime: .qtvr
mhshow-suffix-video/x-msvideo: .avi
mhshow-suffix-video/x-ms-wmv: .wmv
EOF

# I'd like to check if netscape is available and use it preferentially to lynx,
# but only once I've added a new %-escape that makes more permanent temp files,
# so netscape -remote can be used (without -remote you get a complaint dialog
# that another netscape is already running and certain things can't be done).
PGM="`$SEARCHPROG $SEARCHPATH lynx`"
if [ ! -z "$PGM" ]; then
	echo "mhshow-show-text/html: %p$PGM -force-html '%F'" >> $TMP
else
  PGM="`$SEARCHPROG $SEARCHPATH w3m`"
  if [ ! -z "$PGM" ]; then
	echo "mhshow-show-text/html: %p$PGM -T text/html '%F'" >> $TMP
  fi
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
