#! /bin/sh
#
# Generates mh-chart.man from other .man files that have a SYNOPSIS
# section.

nmhmandir=`dirname $0`

cat <<'EOF'
.\"
.\" %nmhwarning%
.\"
.TH MH-CHART %manext1% "%nmhdate%" MH.6.8 [%nmhversion%]
.SH NAME
mh-chart \- Chart of nmh Commands
.SH SYNOPSIS
.na
EOF

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
nmh(1)
EOF
