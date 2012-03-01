BEGIN {IFS="	"; OFS=IFS; FS=IFS}
#234567890123456789012345678901234567890
#define FT_LS_822ADDR   51      /* set "str" to 822 format addr */
{
	if (substr($0,25,2) ~ /[0-9][0-9]/) {
	    a = substr($0,25,2) + 1;
	print substr($0,1,24) sprintf ("%02.2d", a) substr($0,27);
	}
	else print;
}
