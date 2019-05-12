#
#       Proprietary Rand Corporation, 1981.
#       Further distribution of this software
#       subject to the terms of the Rand
#       license agreement.
#

DESTMAN = /usr/man

# 'dist.1' is not ready yet.
MAN1    = \
ali.1 \
comp.1 \
file.1 \
folder.1 \
forw.1 \
inc.1 \
mail.1 \
mh.1 \
mhl.1 \
mhpath.1 \
news.1 \
next.1 \
pick.1 \
prev.1 \
prompter.1 \
repl.1 \
rmail.1 \
rmf.1 \
rmm.1 \
scan.1 \
send.1 \
show.1

MAN5    = \
mh-alias.5 \
mh-mail.5 \
netmail.5 \
mh-profile.5

MAN8    = \
conflict.8 \
deliver.8 \
mailer.8

install: uninstall
	-cp $(MAN1) $(DESTMAN)/man1
	-cp $(MAN5) $(DESTMAN)/man5
	-cp $(MAN8) $(DESTMAN)/man8

uninstall:
	-cd $(DESTMAN)/man1; rm -f $(MAN1)
	-cd $(DESTMAN)/man5; rm -f $(MAN5)
	-cd $(DESTMAN)/man8; rm -f $(MAN8)
