# @(#) $Header: Makefile,v 1.2 89/09/27 03:30:10 leres Exp $ (LBL)

LIBMH= libmh.so.1.1


all: ${LIBMH}


${LIBMH}: ../sbr/libmh.a ../zotnet/libzot.a ../mts/libmts.a 
	rm -rf _tmp
	mkdir _tmp
	cd _tmp ; ar x ../../sbr/libmh.a
	cd _tmp ; ar x ../../zotnet/libzot.a
	cd _tmp ; ar x ../../mts/libmts.a
#	cd _tmp ; ar x ../../uip/libuip.a
	rm -f ${LIBMH}
	cd _tmp ; ld -o ../${LIBMH} -assert pure-text *.o
	rm -rf _tmp

install: ${LIBMH}
	install ${LIBMH} /usr/lib
	ldconfig

clean:
	rm -rf _tmp ${LIBMH}
