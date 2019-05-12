#	%M%	%I%	%E%
#
DESTDIR=

all:	
	cd cmds; make ${MFLAGS}

cp:	all
	cd cmds; make DESTDIR=${DESTDIR} cp
	cd man; make DESTDIR=${DESTDIR} cp

clean:
	cd cmds; make ${MFLAGS} clean
