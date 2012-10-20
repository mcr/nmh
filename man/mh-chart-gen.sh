#!/bin/sh
#
# Generates mh-chart.man from other .man files that have a SYNOPSIS
# section.

nmhmandir=`dirname $0`

# The following ensures the generated date field in the manpage is divorced
# from the local build environment when building distribution packages.
export LC_TIME=C
unset LANG
datestamp=`date '+%B %d, %Y'`

cat <<__HOOPY_FROOD
.TH MH-CHART %manext7% "${datestamp}" "%nmhversion%"
.\"
.\" %nmhwarning%
.\"
.SH NAME
mh-chart \- Chart of nmh Commands
.SH SYNOPSIS
.na
__HOOPY_FROOD

for i in $nmhmandir/*.man; do
  case $i in
    */mh-chart.man) ;;
    *) if grep '^\.ad' "$i" >/dev/null; then
         #### Extract lines from just after .SH SYNOPSIS to just before .ad.
         #### Filter out the "typical usage:" section in pick.man.
         awk '/.SH SYNOPSIS/,/^(\.ad|typical usage:)/ {
                if ($0 !~ /^(\.SH SYNOPSIS|\.na|\.ad|typical usage:)/) print
              }' "$i"
         echo
       fi ;;
  esac
done

cat <<'EOF'
.ad

.SH "SEE ALSO"
nmh(7)
EOF
