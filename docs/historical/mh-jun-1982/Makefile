#
#       Proprietary Rand Corporation, 1981.
#       Further distribution of this software
#       subject to the terms of the Rand
#       license agreement.
#

#

# The following two must match entries in */Makefile
BINDIR  = /usr/local
MHDIR   = /etc/mh

all:
	cd subs; make
	cd strings; make
	cd progs; make progs

uninstall:
	cd progs; make uninstall
	cd support; make uninstall
	cd man; make uninstall
	-rm -rf $(MHDIR)
	-mv /usr/man/man1/bellmail.1 /usr/man/man1/mail.1
	-mv /bin/bellmail /bin/mail
	rm -f /bin/rmail
	ln /bin/mail /bin/rmail
	-mv /usr/man/man1/filetype.1 /usr/man/man1/file.1
	-mv /usr/bin/filetype /usr/bin/file
	-mv /bin/filetype /bin/file

onceonly:
	-@if [ ! -f /usr/bin/filetype -a ! -f /bin/filetype -a -f /bin/file ] ; then \
	  mv /bin/file /bin/filetype;echo "/bin/file => /bin/filetype" ; else true ; fi
	-@if [ ! -f /bin/filetype -a ! -f /usr/bin/filetype -a -f /usr/bin/file ] ; then \
	  mv /usr/bin/file /usr/bin/filetype;echo "/usr/bin/file => /usr/bin/filetype" ; else true ; fi
	-@if [ ! -f /usr/man/man1/filetype.1 -a -f /usr/man/man1/file.1 ] ; then \
	  mv /usr/man/man1/file.1 /usr/man/man1/filetype.1;echo "/usr/man/man1/file.1 => /usr/man/man1/filetype.1" ; else true ; fi
	-@if [ ! -f /bin/bellmail -a -f /bin/mail ] ; then \
	  mv /bin/mail /bin/bellmail;echo "/bin/mail => /bin/bellmail" ; else true ; fi
	-@if [ ! -f /usr/man/man1/bellmail.1 -a -f /usr/man/man1/mail.1 ] ; then \
	  mv /usr/man/man1/mail.1 /usr/man/man1/bellmail.1;echo "/usr/man/man1/mail.1 => /usr/man/man1/bellmail.1" ; else true ; fi
	-@if [ -f $(BINDIR)/comp ] ; then \
		echo "Collision on $(BINDIR)/comp" ; else true ; fi
#       -@if [ -f $(BINDIR)/dist ] ; then \
		echo "Collision on $(BINDIR)/dist" ; else true ; fi
#       -@if [ -f $(BINDIR)/file ] ; then \
		echo "Collision on $(BINDIR)/file" ; else true ; fi
	-@if [ -f $(BINDIR)/folder ] ; then \
		echo "Collision on $(BINDIR)/folder" ; else true ; fi
	-@if [ -f $(BINDIR)/forw ] ; then \
		echo "Collision on $(BINDIR)/forw" ; else true ; fi
	-@if [ -f $(BINDIR)/inc  ] ; then \
		echo "Collision on $(BINDIR)/inc"  ; else true ; fi
#       -@if [ -f $(BINDIR)/mail ] ; then \
		echo "Collision on $(BINDIR)/mail" ; else true ; fi
	-@if [ -f $(BINDIR)/news ] ; then \
		echo "Collision on $(BINDIR)/news" ; else true ; fi
	-@if [ -f $(BINDIR)/next ] ; then \
		echo "Collision on $(BINDIR)/next" ; else true ; fi
	-@if [ -f $(BINDIR)/pick ] ; then \
		echo "Collision on $(BINDIR)/pick" ; else true ; fi
	-@if [ -f $(BINDIR)/prev ] ; then \
		echo "Collision on $(BINDIR)/prev" ; else true ; fi
	-@if [ -f $(BINDIR)/prompter ] ; then \
		echo "Collision on $(BINDIR)/prompter" ; else true ; fi
	-@if [ -f $(BINDIR)/repl ] ; then \
		echo "Collision on $(BINDIR)/repl" ; else true ; fi
	-@if [ -f $(BINDIR)/rmf  ] ; then \
		echo "Collision on $(BINDIR)/rmf"  ; else true ; fi
	-@if [ -f $(BINDIR)/rmm  ] ; then \
		echo "Collision on $(BINDIR)/rmm"  ; else true ; fi
	-@if [ -f $(BINDIR)/scan ] ; then \
		echo "Collision on $(BINDIR)/scan" ; else true ; fi
	-@if [ -f $(BINDIR)/send ] ; then \
		echo "Collision on $(BINDIR)/send" ; else true ; fi
	-@if [ -f $(BINDIR)/show ] ; then \
		echo "Collision on $(BINDIR)/show" ; else true ; fi
	-@if [ -f $(MHDIR) -o -d $(MHDIR) ] ; then \
	     echo "$(MHDIR) already exists!"; else true ; fi

install:
	cd progs; make install
	cd support; make install
	cd man; make install

pinstall:
	cd progs; make pinstall

distribution:
	cd support; make distribution
	cd subs; make distribution
	cd strings; make distribution
	cd progs; make distribution

clean:
	cd support; make clean
	cd subs; make clean
	cd strings; make clean
	cd progs; make clean
