#
#       Proprietary Rand Corporation, 1981.
#       Further distribution of this software
#       subject to the terms of the Rand
#       license agreement.
#


NEWSDIR = /usr/news

install:
	cp 1char           $(NEWSDIR)/.1char
	cp mh_profile      $(NEWSDIR)/.mh_profile
	cp mh_receive      $(NEWSDIR)/.mh_receive

uninstall:
	cd $(NEWSDIR); rm -f .1char .mh_profile .mh_receive
