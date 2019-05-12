#
#       Proprietary Rand Corporation, 1981.
#       Further distribution of this software
#       subject to the terms of the Rand
#       license agreement.
#


# The following must match entry in ../Makefile
MHDIR   = /etc/mh
PROGS   = l c

all:    $(PROGS)

#define TERMCAP in l.c if you have TERMCAP
#otherwise, you'll have to look at l.c and figure out what you want to do
#about the things that will be ifdeffed out
l:      l.c
	-rm -f l
	$(CC) -o l $(CFLAGS) -DTERMCAP l.c -ltermcap

c:      l
	ln l c

install:
	cd news; make install
	-install -c ../support/components $(MHDIR)/components
	-install -c ../support/distcomps $(MHDIR)/distcomps
	-install -c ../support/mhl.format $(MHDIR)/mhl.format

uninstall:
	cd news; make uninstall
	-cd $(MHDIR); rm -f components distcomps mhl.format

distribution: clean

clean:
	-rm $(PROGS)
