##############################################################################
# @(#)Makefile,v 1.1.1.1 1993/01/30 04:41:04 jtc Exp

MAKE	=	make DESTDIR=$(DESTDIR) $(MFLAGS) -k
SHELL	=	/bin/sh


##############################################################################
#	Generation Rules
##############################################################################

.SUFFIXES:	.ms .cat .doc .imp

.ms.cat:;	itroff -t -ms $< > $@

.ms.doc:;	nroff -ms $< > $@

.cat.imp:;	catimp -i $@ $<


######################################################################
#	Here it is...
######################################################################

TARGETS	=	mh-changes.doc

all:		mh-changes.doc


doc:		mh-changes.doc

i-all:	all true
	lpr -t -J MH-changes mh-changes.cat

true:;

##############################################################################
#	Miscellaneous tasks
##############################################################################

distribution:   clean

unconfig clean:		unclean
		rm -f *.cat *.dvi *.imp *.doc

unclean:;	rm -f *.tmp _* :* core
