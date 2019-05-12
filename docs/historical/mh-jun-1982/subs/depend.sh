sed "/DEPENDENCIES/,\$d" < Makefile > Maketemp
echo "###DEPENDENCIES Follow.  Do not delete this line" >> Maketemp
grep \^#include $@ | sed '
	s/\.c/\.o/
	s/:#include/:	/
	s/"\(.*\)"/\1/
	s/<\(.*\)>/\/usr\/include\/\1/
' >> Maketemp
mv Makefile Makeback
mv Maketemp Makefile
