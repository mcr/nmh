#
#       Proprietary Rand Corporation, 1981.
#       Further distribution of this software
#       subject to the terms of the Rand
#       license agreement.
#

LIB = strings.a
OFILES =\
	anoyes.o \
	components.o \
	current.o \
	defalt.o \
	distcomps.o \
	draft.o \
	fileproc.o \
	foldprot.o \
	installproc.o \
	listname.o \
	lockdir.o \
	lproc.o \
	lsproc.o \
	mailboxes.o \
	mailproc.o \
	mh_deliver.o \
	mh_prof.o \
	mhlformat.o \
	mhlstdfmt.o \
	mhnews.o \
	msgprot.o \
	pfolder.o \
	prproc.o \
	rhosts.o \
	scanproc.o \
	sendproc.o \
	showproc.o \
	stdcomps.o \
	stddcomps.o \
	sysed.o \
	sysname.o

CFILES =   anoyes.c \
	   components.c \
	   current.c \
	   defalt.c \
	   distcomps.c \
	   draft.c \
	   fileproc.c \
	   foldprot.c \
	   installproc.c \
	   listname.c \
	   lockdir.c \
	   lproc.c \
	   lsproc.c \
	   mailboxes.c \
	   mailproc.c \
	   mh_deliver.c \
	   mh_prof.c \
	   mhlformat.c \
	   mhlstdfmt.c \
	   mhnews.c \
	   msgprot.c \
	   pfolder.c \
	   prproc.c \
	   rhosts.c \
	   scanproc.c \
	   sendproc.c \
	   showproc.c \
	   stdcomps.c \
	   stddcomps.c \
	   sysed.c \
	   sysname.c

$(LIB): $(OFILES)
	-rm -f $(LIB)
	ar cr $(LIB) $(OFILES)
	ranlib $(LIB)

distribution:   clean
	-rm -f $(LIB)

clean:
	-rm -f $(OFILES)

depend:
	../misc/depend.sh $(CFILES)


###DEPENDENCIES Follow.  Do not delete this line
anoyes.o:       ../mh.h
#lockdir.o:       /usr/include/mailsys.h
#mailboxes.o:     /usr/include/mailsys.h
rhosts.o:	 ../mh.h
#sysname.o:      /usr/include/whoami.h
