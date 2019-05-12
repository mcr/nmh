#	@(#)Makefile	4.9	(Berkeley)	83/07/07
#
# define UCB to get #files instead of ,files
#
LIBS=	subs.a strings.a /usr/src/ucb/libndir/libndir.a
CFLAGS= -O -DUNIXCOMP -DUCB -I. -I/usr/src/ucb/libndir
BINDIR= /usr/new/mh
LIBDIR=	/usr/new/lib
MHDIR=  ${LIBDIR}/mh
SUBS = ../subs
STRINGS = ../strings
PROGS=	comp deliver folder forw inc install-mh \
	next news pick prev prompter refile repl rescue rmf rmm scan \
	send show unixtomh
DESTDIR=

all:	${PROGS}

subs.a: mh.h
	rm -f ${SUBS}/subs.a subs.a
	cd ${SUBS}; make
	ranlib ${SUBS}/subs.a
	ln ${SUBS}/subs.a subs.a

strings.a:
	rm -f ${STRINGS}/strings.a strings.a
	cd ${STRINGS}; make
	ranlib ${STRINGS}/strings.a
	ln ${STRINGS}/strings.a strings.a

comp:   mh.h comp.o ${LIBS}
	${CC} -o comp comp.o ${LIBS}

deliver:mh.h deliver.o ${LIBS}
	${CC} -o deliver deliver.o ${LIBS}

refile:   mh.h refile.o ${LIBS}
	${CC} -o refile refile.o ${LIBS}

folder: mh.h folder.o ${LIBS}
	${CC} -o folder folder.o ${LIBS}

forw:   mh.h forw.o annotate.o ${LIBS}
	${CC} -o forw forw.o annotate.o ${LIBS}

inc:    mh.h inc.o scansub.o ${LIBS}
	${CC} -o inc inc.o scansub.o ${LIBS}

install-mh: mh.h install-mh.o ${LIBS}
	${CC} -o install-mh install-mh.o ${LIBS}

news:   mh.h news.o ${LIBS}
	${CC} -o news news.o ${LIBS}

next:   mh.h nexthdr.o next.o ${LIBS}
	${CC} -o next nexthdr.o next.o ${LIBS}

pick:   mh.h pick.o grep.o ${LIBS}
	${CC} -o pick pick.o grep.o ${LIBS}

prev:   mh.h prevhdr.o next.o ${LIBS}
	${CC} -o prev prevhdr.o next.o ${LIBS}

prompter: mh.h prompter.o ${LIBS}
	${CC} -o prompter prompter.o ${LIBS}

repl:   mh.h reply.o replsubs.o annotate.o ${LIBS}
	${CC} -o repl reply.o replsubs.o annotate.o ${LIBS}

rescue:	rescue.o emitl.o
	${CC} -o rescue rescue.o

rmf:    mh.h rmf.o ${LIBS}
	${CC} -o rmf rmf.o ${LIBS}

rmm:    mh.h rmm.o ${LIBS}
	${CC} -o rmm rmm.o ${LIBS}

scan:   mh.h scan.o scansub.o ${LIBS}
	${CC} -o scan scan.o scansub.o ${LIBS}

send:   mh.h send.o ${LIBS}
	${CC} -o send send.o ${LIBS}

show:   mh.h show.o ${LIBS}
	${CC} -o show show.o ${LIBS}

unixtomh: unixtomh.o
	${CC} -o unixtomh unixtomh.o subs.a

onceonly:
	-@if [ ! -f ${DESTDIR}/usr/bin/filetype -a \
	       ! -f ${DESTDIR}/bin/filetype -a -f ${DESTDIR}/bin/file ] ; then \
		mv ${DESTDIR}/bin/file ${DESTDIR}/bin/filetype; \
		echo "${DESTDIR}/bin/file => ${DESTDIR}/bin/filetype" ; \
	 else true ; \
	 fi
	-@if [ ! -f ${DESTDIR}/bin/filetype -a \
	       ! -f ${DESTDIR}/usr/bin/filetype -a \
		 -f ${DESTDIR}/usr/bin/file ] ; then \
		mv ${DESTDIR}/usr/bin/file ${DESTDIR}/usr/bin/filetype; \
		echo "${DESTDIR}/usr/bin/file => ${DESTDIR}/usr/bin/filetype"; \
	 else true ; \
	 fi
	-@if [ ! -f ${DESTDIR}/bin/bellmail -a -f ${DESTDIR}/bin/mail ] ; then \
		mv ${DESTDIR}/bin/mail ${DESTDIR}/bin/bellmail; \
		echo "${DESTDIR}/bin/mail => ${DESTDIR}/bin/bellmail" ;\
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/comp ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/comp" ; \
	 else true ; \
	 fi
#       -@if [ -f ${DESTDIR}/${BINDIR}/dist ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/dist" ; \
	 else true ; \
	 fi
#       -@if [ -f ${DESTDIR}/${BINDIR}/file ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/file" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/folder ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/folder" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/forw ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/forw" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/inc  ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/inc"  ; \
	 else true ; \
	 fi
#       -@if [ -f ${DESTDIR}/${BINDIR}/mail ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/mail" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/news ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/news" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/next ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/next" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/pick ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/pick" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/prev ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/prev" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/prompter ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/prompter" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/repl ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/repl" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/rmf  ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/rmf"  ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/rmm  ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/rmm"  ; \
	  else true ; \
	  fi
	-@if [ -f ${DESTDIR}/${BINDIR}/scan ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/scan" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/send ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/send" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${BINDIR}/show ] ; then \
		echo "Collision on ${DESTDIR}/${BINDIR}/show" ; \
	 else true ; \
	 fi
	-@if [ -f ${DESTDIR}/${MHDIR} -o -d ${DESTDIR}/${MHDIR} ] ; then \
	     echo "${DESTDIR}/${MHDIR} already exists!"; \
	 else true ; \
	 fi

cp:	all
	-mkdir ${DESTDIR}/${BINDIR}
	-mkdir ${DESTDIR}/${LIBDIR}
	-mkdir ${DESTDIR}/${MHDIR}
	cp comp ${DESTDIR}/${BINDIR}; chmod 1755 ${DESTDIR}/${BINDIR}/comp ; strip ${DESTDIR}/${BINDIR}/comp
	cp deliver ${DESTDIR}/${MHDIR}; strip ${DESTDIR}/${MHDIR}/deliver; chmod 4755 ${DESTDIR}/${MHDIR}/deliver
	cp folder ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/folder
	cp forw ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/forw
	cp inc ${DESTDIR}/${BINDIR}; chmod 1755 ${DESTDIR}/${BINDIR}/inc ; strip ${DESTDIR}/${BINDIR}/inc
	cp install-mh ${DESTDIR}/${MHDIR}; strip ${DESTDIR}/${MHDIR}/install-mh
	cp next ${DESTDIR}/${BINDIR}; chmod 1755 ${DESTDIR}/${BINDIR}/next ; strip ${DESTDIR}/${BINDIR}/next
	cp news ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/news
	cp pick ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/pick
	cp prev ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/prev
	cp prompter ${DESTDIR}/${BINDIR}; chmod 1755 ${DESTDIR}/${BINDIR}/prompter ; strip ${DESTDIR}/${BINDIR}/prompter
	cp refile ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/refile
	cp repl ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/repl
	cp rmf ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/rmf
	cp rmm ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/rmm
	cp scan ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/scan
	cp send ${DESTDIR}/${BINDIR}; strip ${DESTDIR}/${BINDIR}/send
	cp show ${DESTDIR}/${BINDIR}; chmod 1755 ${DESTDIR}/${BINDIR}/show ; strip ${DESTDIR}/${BINDIR}/show
	cp unixtomh ${DESTDIR}/${MHDIR}; strip ${DESTDIR}/${MHDIR}/unixtomh
	rm -f ${DESTDIR}/${BINDIR}/folders
	ln ${DESTDIR}/${BINDIR}/folder ${DESTDIR}/${BINDIR}/folders
	cp ../support/components ${DESTDIR}/${MHDIR}/components;
	cp ../support/distcomps ${DESTDIR}/${MHDIR}/distcomps;

cleanup:
	rm -f ${PROGS}

clean:
	cd ${SUBS}; make ${MFLAG} clean
	cd ${STRINGS}; make ${MFLAG} clean
	rm -f ${PROGS} subs.a strings.a *.o
