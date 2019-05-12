#
#       Proprietary Rand Corporation, 1981.
#       Further distribution of this software
#       subject to the terms of the Rand
#       license agreement.
#

#                       Remove the -DRAND elsewhere!
# In ../mh.h: define ARPANET if you are on the arpanet.
# In ../mh.h: define VMUNIX if you are running a berkeley system
JOBSLIB=#-ljobs

CFLAGS  = -O $(A) -I/usr/include -I../support # -m -DRAND
# On a PDP/11, if the sticky bit is to be set, text must be shareable
CCSHARE = -n

# The following two must match entry in ../Makefile
BINDIR  = /usr/local
MHDIR   = /etc/mh
MHTMP   = /usr/tmp/mh
LOCKDIR = /usr/spool/locks
NETDIR  = /usr/spool/netmail

SUBS    = ../subs/subs.a
STRINGS = ../strings/strings.a

LINT    = lint

# The 'dist' program is not ready yet, and so is not listed in the CMDS
# See also ../man/Makefile
CMDS    = \
ali \
comp \
file \
folder \
forw \
inc \
mail \
mhpath \
next \
news \
pick \
prev \
prompter \
repl \
rmf \
rmm \
scan \
send \
show

MISC    = \
conflict \
deliver \
install-mh \
mhl

PROGS = $(CMDS) $(MISC) rmail

CFILES  = \
adrformat.c \
adrparse.c \
ali.c \
comp.c \
conflict.c \
deliver.c \
dist.c \
file.c \
folder.c \
forw.c \
inc.c \
install-mh.c \
mail.c \
mhl.c \
mhpath.c \
news.c \
next.c \
pick.c \
prompter.c \
repl.c \
rmail.c \
rmf.c \
rmm.c \
scan.c \
scansub.c \
send.c \
show.c

progs:  $(PROGS)

ali:    ali.o $(SUBS)
	$(CC) -o ali ali.o $(SUBS)

comp:   comp.o $(SUBS) $(STRINGS)
	$(CC) $(CCSHARE) -o comp comp.o $(SUBS) $(STRINGS)

conflict: conflict.o $(SUBS)
	$(CC) -o conflict conflict.o $(SUBS)

deliver: deliver.o adrparse.o adrformat.o $(SUBS) $(STRINGS)
	$(CC) $(CCSHARE) -o deliver deliver.o adrparse.o adrformat.o $(SUBS) \
	$(STRINGS) $(JOBSLIB)

dist:   dist.o annotate.o $(SUBS) $(STRINGS)
	$(CC) -o dist dist.o annotate.o $(SUBS) $(STRINGS)

file:   file.o $(SUBS) $(STRINGS)
	$(CC) -o file file.o $(SUBS) $(STRINGS)

folder: folder.o $(SUBS) $(STRINGS)
	$(CC) -o folder folder.o $(SUBS) $(STRINGS)

forw:   forw.o annotate.o $(SUBS) $(STRINGS)
	$(CC) -o forw forw.o annotate.o $(SUBS) $(STRINGS)

inc:    inc.o scansub.o $(SUBS) $(STRINGS)
	$(CC) $(CCSHARE) -o inc inc.o scansub.o $(SUBS) $(STRINGS)

install-mh: install-mh.o $(SUBS) $(STRINGS)
	$(CC) -o install-mh install-mh.o $(SUBS) $(STRINGS)

mail:   mail.o $(SUBS) $(STRINGS)
	$(CC) -o mail mail.o $(SUBS) $(STRINGS)

mhpath: mhpath.o $(SUBS) $(STRINGS)
	$(CC) -o mhpath mhpath.o $(SUBS) $(STRINGS)

mhl:    mhl.o $(SUBS) $(STRINGS)
	$(CC) -o mhl mhl.o $(SUBS) $(STRINGS)

news:   news.o $(SUBS) $(STRINGS)
	$(CC) -o news news.o $(SUBS) $(STRINGS)

next:   nexthdr.o next.o $(SUBS) $(STRINGS)
	$(CC) $(CCSHARE) -o next nexthdr.o next.o $(SUBS) $(STRINGS)

pick:   pick.o grep.o $(SUBS) $(STRINGS)
	$(CC) -o pick pick.o grep.o $(SUBS) $(STRINGS)

prev:   prevhdr.o next.o $(SUBS) $(STRINGS)
	$(CC) -o prev prevhdr.o next.o $(SUBS) $(STRINGS)

prompter: prompter.o $(SUBS) $(STRINGS)
	$(CC) $(CCSHARE) -o prompter prompter.o $(SUBS) $(STRINGS)

repl:   repl.o annotate.o adrparse.o adrformat.o $(SUBS) $(STRINGS)
	$(CC) -o repl repl.o annotate.o adrparse.o adrformat.o $(SUBS) $(STRINGS)

rmail:  rmail.o  $(SUBS) $(STRINGS)
	$(CC) -o rmail rmail.o  $(SUBS) $(STRINGS)

rmf:    rmf.o $(SUBS) $(STRINGS)
	$(CC) -o rmf rmf.o $(SUBS) $(STRINGS)

rmm:    rmm.o $(SUBS) $(STRINGS)
	$(CC) -o rmm rmm.o $(SUBS) $(STRINGS)

scan:   scan.o scansub.o $(SUBS) $(STRINGS)
	$(CC) -o scan scan.o scansub.o $(SUBS) $(STRINGS)

send:   send.o $(SUBS) $(STRINGS)
	$(CC) -o send send.o $(SUBS) $(STRINGS)

show:   show.o $(SUBS) $(STRINGS)
	$(CC) $(CCSHARE) -o show show.o $(SUBS) $(STRINGS)

lint:
	-$(LINT) ali.c -v ../subs/subs-lc | tee ali.lint
	-$(LINT) comp.c -v ../subs/subs-lc | tee comp.lint
	-$(LINT) conflict.c -v ../subs/subs-lc | tee conflict.lint
	-$(LINT) deliver.c adrparse.c adrformat.c -v ../subs/subs-lc | tee deliver.lint
	-$(LINT) dist.c -v ../subs/subs-lc | tee dist.lint
	-$(LINT) file.c -v ../subs/subs-lc | tee file.lint
	-$(LINT) folder.c -v ../subs/subs-lc | tee folder.lint
	-$(LINT) forw.c annotate.c -v ../subs/subs-lc | tee forw.lint
	-$(LINT) inc.c scansub.c -v ../subs/subs-lc | tee inc.lint
	-$(LINT) install-mh.c -v ../subs/subs-lc | tee install.lint
	-$(LINT) mail.c -v ../subs/subs-lc | tee mail.lint
	-$(LINT) mhl.c -v ../subs/subs-lc | tee mhl.lint
	-$(LINT) mhpath.c -v ../subs/subs-lc | tee mhpath.lint
	-$(LINT) news.c -v ../subs/subs-lc | tee news.lint
	-$(LINT) pick.c -v ../subs/subs-lc | tee pick.lint
	-$(LINT) prevhdr.c next.c -v ../subs/subs-lc | tee prev.lint
	-$(LINT) prompter.c -v ../subs/subs-lc | tee prompter.lint
	-$(LINT) repl.c replsubs.c -v ../subs/subs-lc | tee repl.lint
	-$(LINT) rmf.c -v ../subs/subs-lc | tee rmf.lint
	-$(LINT) rmm.c -v ../subs/subs-lc | tee rmm.lint
	-$(LINT) scan.c scansub.c -v ../subs/subs-lc | tee scan.lint
	-$(LINT) show.c -v ../subs/subs-lc | tee show.lint

install: strip installprogs
	-mkdir $(MHTMP)
	-mkdir $(MHDIR)
	-mkdir $(LOCKDIR)
	-mkdir $(NETDIR)
	-chmod a+x $(MISC)
	-cd $(MHDIR); rm -f $(MISC)
	-cp $(MISC) $(MHDIR)
	echo Now become root and \'make installroot\'

installprogs: chmod
	-cd $(BINDIR); rm -f $(CMDS) folders
	-cp $(CMDS) $(BINDIR)
	-cd $(BINDIR); ln folder folders

installroot:
	-chmod 777 $(MHTMP)
	-cd $(BINDIR); chmod a+x $(CMDS)
	-cd $(MHDIR); chmod a+x $(MISC)
	/etc/chown root $(NETDIR)
	chmod 755  $(NETDIR)
	-rm -f /bin/rmail
	/etc/chown root $(LOCKDIR)
	chmod 777  $(LOCKDIR)
	-cp rmail /bin
#           Setuid and Setgid:
	-chmod 6755 /bin/rmail
#           Sticky Bit & Setuid:
	-/etc/chown root $(MHDIR)/deliver;chmod 5755 $(MHDIR)/deliver
#           Sticky Bit:
	-chmod 1755 $(BINDIR)/comp
	-chmod 1755 $(BINDIR)/inc
	-chmod 1755 $(BINDIR)/next
	-chmod 1755 $(BINDIR)/prompter
	-chmod 1755 $(BINDIR)/show

uninstall:
	-rmdir $(MHTMP)
	-cd $(MHDIR); rm -f $(MISC)
	-cd $(BINDIR); rm -f $(CMDS) folders

strip:
	-strip $(PROGS)

chmod:
	chmod a+x $(PROGS)

pinstall:
	-install -s comp $(BINDIR)/comp
	-install -s file $(BINDIR)/file
	-install -s folder $(BINDIR)/folder
	-install -s forw $(BINDIR)/forw
	-install -s inc $(BINDIR)/inc
	-install -s mhl $(MHDIR)/mhl
	-install -s mhpath $(BINDIR)/mhpath
	-install -s next $(BINDIR)/next
	-install -s pick $(BINDIR)/pick
	-install -s prev $(BINDIR)/prev
	-install -s prompter $(BINDIR)/prompter
	-install -s repl $(BINDIR)/repl
	-install -s rmf $(BINDIR)/rmf
	-install -s rmm $(BINDIR)/rmm
	-install -s scan $(BINDIR)/scan
	-install -s send $(BINDIR)/send
	-install -s show $(BINDIR)/show

distribution:   clean rmprogs

rmprogs:
	-rm -f $(PROGS)

clean:
	-rm -f *.o *.lint

depend:
	../misc/depend.sh $(CFILES)


###DEPENDENCIES Follow.  Do not delete this line
adrformat.o:     /usr/include/stdio.h
adrformat.o:     ../mh.h
adrformat.o:     ../adrparse.h
adrformat.o:     /usr/include/ctype.h
adrparse.o:      /usr/include/stdio.h
#adrparse.o:      /usr/include/whoami.h
adrparse.o:      ../mh.h
adrparse.o:      ../adrparse.h
#adrparse.o:      /usr/include/imp.h
adrparse.o:      /usr/include/ctype.h
ali.o:   ../mh.h
ali.o:   /usr/include/stdio.h
ali.o:   /usr/include/ctype.h
ali.o:   /usr/include/pwd.h
ali.o:   /usr/include/sys/types.h
comp.o:  ../mh.h
comp.o:  /usr/include/stdio.h
#comp.o:  /usr/include/strings.h
comp.o:  /usr/include/signal.h
conflict.o:      /usr/include/stdio.h
conflict.o:      /usr/include/ctype.h
conflict.o:      /usr/include/pwd.h
conflict.o:      /usr/include/grp.h
conflict.o:      /usr/include/sys/types.h
conflict.o:      /usr/include/sys/dir.h
conflict.o:      ../mh.h
#conflict.o:      /usr/include/mailsys.h
#deliver.o:       /usr/include/whoami.h
deliver.o:       ../mh.h
deliver.o:       ../adrparse.h
deliver.o:       /usr/include/stdio.h
deliver.o:       /usr/include/ctype.h
deliver.o:       /usr/include/pwd.h
deliver.o:       /usr/include/sys/types.h
deliver.o:       /usr/include/sys/timeb.h
deliver.o:       /usr/include/sys/stat.h
deliver.o:       /usr/include/signal.h
#deliver.o:       /usr/include/strings.h
#deliver.o:       /usr/include/mailsys.h
deliver.o:       /usr/include/time.h
dist.o:  ../mh.h
dist.o:  /usr/include/stdio.h
dist.o:  /usr/include/signal.h
#dist.o:  /usr/include/strings.h
dist.o:  /usr/include/sys/types.h
dist.o:  /usr/include/sys/stat.h
file.o:  ../mh.h
file.o:  ../folder.h
file.o:  /usr/include/stdio.h
folder.o:        ../mh.h
folder.o:        /usr/include/stdio.h
folder.o:        /usr/include/sys/types.h
folder.o:        /usr/include/sys/stat.h
#folder.o:        /usr/include/strings.h
forw.o:  ../mh.h
forw.o:  /usr/include/stdio.h
forw.o:  /usr/include/signal.h
#forw.o:  /usr/include/strings.h
forw.o:  /usr/include/sys/types.h
forw.o:  /usr/include/sys/stat.h
inc.o:   ../mh.h
inc.o:   /usr/include/stdio.h
inc.o:   /usr/include/sys/types.h
inc.o:   /usr/include/sys/stat.h
inc.o:   /usr/include/errno.h
#inc.o:   /usr/include/strings.h
inc.o:   /usr/include/signal.h
inc.o:   scansub.h
install-mh.o:    ../mh.h
install-mh.o:    /usr/include/stdio.h
install-mh.o:    /usr/include/sys/types.h
install-mh.o:    /usr/include/sys/stat.h
mail.o:  ../mh.h
mail.o:  /usr/include/stdio.h
mail.o:  /usr/include/signal.h
mhl.o:   /usr/include/ctype.h
mhl.o:   /usr/include/signal.h
mhl.o:   /usr/include/setjmp.h
mhl.o:   /usr/include/sgtty.h
mhl.o:   /usr/include/stdio.h
mhl.o:   ../mh.h
mhpath.o:        ../mh.h
mhpath.o:        /usr/include/stdio.h
#mhpath.o:        /usr/include/strings.h
mhpath.o:        /usr/include/ctype.h
news.o:  ../mh.h
news.o:  /usr/include/stdio.h
news.o:  /usr/include/sys/types.h
news.o:  /usr/include/sys/stat.h
news.o:  /usr/include/sys/dir.h
#news.o:  /usr/include/strings.h
news.o:  /usr/include/sys/timeb.h
next.o:  ../mh.h
next.o:  /usr/include/stdio.h
#next.o:  /usr/include/strings.h
pick.o:  ../mh.h
pick.o:  ../folder.h
pick.o:  /usr/include/stdio.h
pick.o:  /usr/include/signal.h
pick.o:  /usr/include/sys/types.h
pick.o:  /usr/include/sys/stat.h
prompter.o:      ../mh.h
prompter.o:      /usr/include/stdio.h
prompter.o:      /usr/include/errno.h
prompter.o:      /usr/include/sgtty.h
prompter.o:      /usr/include/signal.h
#prompter.o:      /usr/include/strings.h
#repl.o:  /usr/include/whoami.h
repl.o:  ../mh.h
repl.o:  /usr/include/stdio.h
repl.o:  /usr/include/signal.h
#repl.o:  /usr/include/strings.h
repl.o:  /usr/include/sys/types.h
repl.o:  /usr/include/sys/stat.h
repl.o:  ../adrparse.h
rmail.o:         ../mh.h
#rmail.o:         /usr/include/whoami.h
rmail.o:         /usr/include/stdio.h
rmail.o:         /usr/include/sys/types.h
rmail.o:         /usr/include/sys/timeb.h
rmail.o:         /usr/include/time.h
rmf.o:   ../mh.h
rmf.o:   /usr/include/stdio.h
#rmf.o:   /usr/include/strings.h
rmm.o:   ../mh.h
rmm.o:   /usr/include/stdio.h
#rmm.o:   /usr/include/strings.h
scan.o:  ../mh.h
scan.o:  /usr/include/stdio.h
#scan.o:  /usr/include/strings.h
scan.o:  scansub.h
scansub.o:       ../mh.h
#scansub.o:       /usr/include/whoami.h
scansub.o:       /usr/include/stdio.h
scansub.o:       /usr/include/ctype.h
scansub.o:       /usr/include/time.h
scansub.o:       ../adrparse.h
scansub.o:       scansub.h
send.o:  ../mh.h
send.o:  /usr/include/stdio.h
send.o:  /usr/include/sys/types.h
send.o:  /usr/include/stat.h
#send.o:  /usr/include/strings.h
send.o:  /usr/include/signal.h
show.o:  ../mh.h
show.o:  /usr/include/stdio.h
#show.o:  /usr/include/strings.h