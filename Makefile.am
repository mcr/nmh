##
## Our Makefile.am template for Makefile.in (and, eventually, Makefile)
##
## Process this with automake to generate Makefile.in
##

## We set this to get our autoconf macros in the m4 directory
ACLOCAL_AMFLAGS = -I m4

##
## This is the default set of libraries all programs link against.  Some
## programs add extra libraries to this set, so they override this with
## a program-specific LDADD variable.  Note that in all of the cases today
## LDADD is included in the program-specific LDADD since we want to add
## to this list of libraries, not replace it.
##
LDADD = sbr/libmh.a sbr/libdtimep.a

##
## These are used to create the default mhn.defaults config file
## MHNSEARCHPATH is the default path to look for MIME content handlers
## MHNSEARCHPROG is the script used to find the support programs
##
MHNSEARCHPATH = "$(PATH):/usr/demo/SOUND"
MHNSEARCHPROG = $(srcdir)/etc/mhn.find.sh

##
## Automake doesn't let us install programs in $(libdir), so we create our
## own directory prefix to use in the primary variable.
##
auxexecdir = @libdir@

##
## nmh _does_ have a test suite!
##
TESTS_ENVIRONMENT = MH_OBJ_DIR=@abs_builddir@ \
		    MH_TEST_DIR=@abs_builddir@/test/testdir \
		    auxexecdir=$(auxexecdir) bindir=$(bindir) \
		    mandir=$(mandir) sysconfdir=$(sysconfdir)
##
## Important note: the "cleanup" test should always be last
##
TESTS = test/bad-input/test-header \
	test/burst/test-burst \
	test/comp/test-comp-format \
	test/folder/test-create \
	test/folder/test-total test/format/test-localmbox \
	test/format/test-myname test/format/test-myhost \
	test/format/test-mymbox \
	test/forw/test-forw-digest \
	test/forw/test-forw-format \
	test/inc/test-deb359167 test/inc/test-eom-align \
	test/inc/test-inc-scanout \
	test/manpages/test-manpages test/mhbuild/test-forw \
	test/mhlist/test-mhlist test/mhparam/test-mhparam \
	test/mhpath/test-mhpath \
	test/mhshow/test-cte-binary test/mhshow/test-qp \
	test/mhshow/test-subpart test/mhstore/test-mhstore \
	test/new/test-basic \
	test/pick/test-pick test/pick/test-stderr \
	test/post/test-post-basic test/post/test-post-multiple \
	test/post/test-post-dcc test/post/test-post-fcc \
	test/post/test-post-multifrom test/post/test-post-envelope \
	test/post/test-post-group \
	test/refile/test-refile \
	test/repl/test-if-str test/repl/test-trailing-newline \
	test/repl/test-multicomp \
	test/scan/test-scan \
	test/sequences/test-flist test/sequences/test-mark \
	test/whatnow/test-attach-detach test/whatnow/test-cd \
	test/whatnow/test-ls test/whom/test-whom \
	test/cleanup ## The "cleanup" test should always be last.

check_SCRIPTS = test/common.sh
check_PROGRAMS = test/getfullname test/getfqdn test/fakesmtp

##
## Stuff that should be cleaned via "make clean"
##
CLEANFILES = config/version.c sbr/sigmsg.h etc/mts.conf etc/sendfiles \
	     etc/mhn.defaults man/man.sed man/mh-chart.man $(man_MANS)
clean-local:
	@rm -rf RPM a.out.DSYM uip/a.out.DSYM
	@rm -rf test/testdir

##
## Stuff that should be cleaned via "make maintainer-clean"
##
MAINTAINERCLEANFILES = cscope.files cscope.out

##
## And our own superclean, to get everything left by maintainer-clean.
##
SUPERCLEANFILES = ChangeLog Makefile.in aclocal.m4 compile config.guess \
		  config.h.in config.h.in~ config.sub configure depcomp \
		  install-sh missing ylwrap nmh-*.tar.gz
superclean: maintainer-clean
	@rm -f $(SUPERCLEANFILES)
.PHONY: superclean

##
## Files that need to be built before everything else
##
BUILT_SOURCES = sbr/sigmsg.h

##
## This is a list of all programs that get installed in the "bin" directory
## Note that primary difference between "PROGRAMS" and "SCRIPTS" is that
## the latter do not have $(EXEEXT) added on the end.
##
bin_PROGRAMS = uip/ali uip/anno uip/burst uip/comp uip/dist uip/flist \
	       uip/folder uip/forw uip/inc uip/install-mh uip/mark \
	       uip/mhbuild uip/mhlist uip/mhmail uip/mhn uip/mhparam \
	       uip/mhpath uip/mhshow uip/mhstore uip/msgchk uip/msh uip/new \
	       uip/packf uip/pick uip/prompter uip/refile uip/repl uip/rmf \
	       uip/rmm uip/scan uip/send uip/show uip/sortm uip/whatnow \
	       uip/whom

bin_SCRIPTS = etc/sendfiles

##
## This is all programs that get installed in the "lib" directory
##
auxexec_PROGRAMS = uip/ap uip/conflict uip/dp uip/fmtdump uip/mhl uip/post \
		   uip/rcvdist uip/rcvpack uip/rcvstore uip/rcvtty uip/slocal \
		   uip/spost uip/viamail uip/mhtest

##
## Internal libraries that we create as part of the build process
## but do not install
##
noinst_LIBRARIES = sbr/libmh.a sbr/libdtimep.a mts/libmts.a

##
## These are all of our header files.  Right now we don't install any of
## them, but that might change in the future.
##
noinst_HEADERS = h/addrsbr.h h/aliasbr.h h/crawl_folders.h h/dropsbr.h \
		 h/fmt_compile.h h/fmt_scan.h h/m_setjmp.h h/md5.h h/mf.h \
		 h/mh.h h/mhcachesbr.h h/mhparse.h h/mime.h h/msh.h \
		 h/mts.h h/nmh.h h/picksbr.h h/popsbr.h h/prototypes.h \
		 h/rcvmail.h h/scansbr.h h/signals.h h/tws.h h/utils.h \
		 h/vmhsbr.h mts/smtp/smtp.h

##
## Extra files we need to install in various places
##
dist_sysconf_DATA = etc/MailAliases etc/components etc/digestcomps \
		    etc/distcomps etc/forwcomps etc/mhl.body etc/mhl.digest \
		    etc/mhl.format etc/mhl.forward etc/mhl.headers \
		    etc/mhl.reply etc/rcvdistcomps etc/rcvdistcomps.outbox \
		    etc/replcomps etc/replgroupcomps etc/scan.MMDDYY \
		    etc/scan.YYYYMMDD etc/scan.default etc/scan.mailx \
		    etc/scan.nomime etc/scan.size etc/scan.time \
		    etc/scan.timely etc/scan.unseen

##
## The same as above, but we don't include these in the distribution
## (because they're generated at compile time)
##
sysconf_DATA = etc/mhn.defaults etc/mts.conf

##
## Documentation that gets installed in docdir
##
dist_doc_DATA = COPYRIGHT VERSION docs/COMPLETION-BASH docs/COMPLETION-TCSH \
		docs/COMPLETION-ZSH docs/DIFFERENCES docs/FAQ \
		docs/MAIL.FILTERING docs/MAILING-LISTS docs/README-ATTACHMENTS \
		docs/README-HOOKS docs/README-components docs/README.about \
		docs/README.SASL docs/README.developers docs/README.manpages \
		docs/TODO

##
## Our man pages
##
man_MANS = man/ali.1 man/anno.1 man/ap.8 man/burst.1 man/comp.1 \
	   man/conflict.8 man/dist.1 man/dp.8 man/flist.1 man/flists.1 \
	   man/fmtdump.8 man/fnext.1 man/folder.1 man/folders.1 \
	   man/forw.1 man/fprev.1 man/inc.1 man/install-mh.1 man/mark.1 \
	   man/mh-alias.5 man/mh-chart.1 man/mh-draft.5 man/mh-format.5 \
	   man/mh-mail.5 man/mh-profile.5 man/mh-sequence.5 \
	   man/mh-tailor.5 man/mhbuild.1 man/mhl.1 man/mhlist.1 \
	   man/mhmail.1 man/mhn.1 man/mhparam.1 man/mhpath.1 \
	   man/mhshow.1 man/mhstore.1 man/msgchk.1 man/msh.1 \
	   man/mts.conf.5 man/new.1 man/next.1 man/nmh.1 man/packf.1 \
	   man/pick.1 man/post.8 man/prev.1 man/prompter.1 man/rcvdist.1 \
	   man/rcvpack.1 man/rcvstore.1 man/rcvtty.1 man/refile.1 \
	   man/repl.1 man/rmf.1 man/rmm.1 man/scan.1 man/send.1 \
	   man/sendfiles.1 man/show.1 man/slocal.1 man/sortm.1 man/unseen.1 \
	   man/whatnow.1 man/whom.1

##
## Files we need to include in the distribution which aren't found by
## Automake using the automatic rules
##
EXTRA_DIST = config/version.sh sbr/sigmsg.awk etc/mts.conf.in etc/sendfiles.in \
	     etc/mhn.defaults.sh $(MHNSEARCHPROG) nmh.spec DATE \
	     docs/ChangeLog_MH-3_to_MH-6.6 \
	     docs/ChangeLog_MH-6.7.0_to_MH-6.8.4.html \
	     man/ali.man man/anno.man man/ap.man man/burst.man man/comp.man \
	     man/conflict.man man/dist.man man/dp.man man/flist.man \
	     man/flists.man man/fmtdump.man man/fnext.man man/folder.man \
	     man/folders.man man/forw.man man/fprev.man man/inc.man \
	     man/install-mh.man man/mark.man man/mh-alias.man \
	     man/mh-chart-gen.sh man/mh-draft.man man/mh-format.man \
	     man/mh-mail.man man/mh-profile.man man/mh-sequence.man \
	     man/mh-tailor.man man/mhbuild.man man/mhl.man man/mhlist.man \
	     man/mhmail.man man/mhn.man man/mhparam.man man/mhpath.man \
	     man/mhshow.man man/mhstore.man man/msgchk.man man/msh.man \
	     man/mts.conf.man man/new.man man/next.man man/nmh.man \
	     man/packf.man man/pick.man man/post.man man/prev.man \
	     man/prompter.man man/rcvdist.man man/rcvpack.man \
	     man/rcvstore.man man/rcvtty.man man/refile.man man/repl.man \
	     man/rmf.man man/rmm.man man/scan.man man/send.man \
	     man/sendfiles.man man/show.man man/slocal.man man/sortm.man \
	     man/unseen.man man/whatnow.man man/whom.man \
	     test/README $(TESTS) test/inc/deb359167.mbox \
	     test/inc/fromline.txt test/inc/msgheader.txt test/inc/filler.txt \
	     test/inc/md5sums test/post/test-post-common.sh

##
## These are all of the definitions for each of the programs listed above.
## They describe the source files that make up the programs and any particular
## overrides that are used to select different library options from the
## default.
##
uip_ali_SOURCES = uip/ali.c uip/aliasbr.c

uip_anno_SOURCES = uip/anno.c uip/annosbr.c

uip_burst_SOURCES = uip/burst.c

uip_comp_SOURCES = uip/comp.c uip/whatnowproc.c uip/whatnowsbr.c uip/sendsbr.c \
		   uip/annosbr.c uip/distsbr.c
uip_comp_LDADD = $(LDADD) $(ICONVLIB) $(READLINELIB)

uip_dist_SOURCES = uip/dist.c uip/whatnowproc.c uip/whatnowsbr.c uip/sendsbr.c \
		   uip/annosbr.c uip/distsbr.c uip/forwsbr.c
uip_dist_LDADD = $(LDADD) $(ICONVLIB) $(READLINELIB)

uip_flist_SOURCES = uip/flist.c

uip_folder_SOURCES = uip/folder.c

uip_forw_SOURCES = uip/forw.c uip/whatnowproc.c uip/whatnowsbr.c uip/sendsbr.c \
		   uip/annosbr.c uip/distsbr.c uip/forwsbr.c
uip_forw_LDADD = $(LDADD) $(ICONVLIB) $(READLINELIB)

uip_inc_SOURCES = uip/inc.c uip/scansbr.c uip/dropsbr.c uip/termsbr.c \
		  uip/popsbr.c
uip_inc_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(SASLLIB)

uip_install_mh_SOURCES = uip/install-mh.c

uip_mark_SOURCES = uip/mark.c

uip_mhbuild_SOURCES = uip/mhbuild.c uip/mhbuildsbr.c uip/mhcachesbr.c \
		      uip/mhlistsbr.c uip/mhoutsbr.c uip/mhmisc.c  \
		      uip/mhfree.c uip/mhparse.c uip/termsbr.c uip/md5.c
uip_mhbuild_LDADD = $(LDADD) $(TERMLIB)

uip_mhlist_SOURCES = uip/mhlist.c uip/mhparse.c uip/mhcachesbr.c \
		     uip/mhlistsbr.c uip/mhmisc.c uip/mhfree.c uip/termsbr.c \
		     uip/md5.c
uip_mhlist_LDADD = $(LDADD) $(TERMLIB)

uip_mhmail_SOURCES = uip/mhmail.c

uip_mhn_SOURCES = uip/mhn.c uip/mhparse.c uip/mhcachesbr.c uip/mhshowsbr.c \
		  uip/mhlistsbr.c uip/mhstoresbr.c uip/mhmisc.c uip/mhfree.c \
		  uip/termsbr.c uip/md5.c
uip_mhn_LDADD = $(LDADD) $(TERMLIB)

uip_mhparam_SOURCES = uip/mhparam.c

uip_mhpath_SOURCES = uip/mhpath.c

uip_mhshow_SOURCES = uip/mhshow.c uip/mhparse.c uip/mhcachesbr.c \
		     uip/mhshowsbr.c uip/mhlistsbr.c uip/mhmisc.c \
		     uip/mhfree.c uip/termsbr.c uip/md5.c
uip_mhshow_LDADD = $(LDADD) $(TERMLIB)

uip_mhstore_SOURCES = uip/mhstore.c uip/mhparse.c uip/mhcachesbr.c \
		      uip/mhshowsbr.c uip/mhlistsbr.c uip/mhstoresbr.c \
		      uip/mhmisc.c uip/mhfree.c uip/termsbr.c uip/md5.c
uip_mhstore_LDADD = $(LDADD) $(TERMLIB)

uip_msgchk_SOURCES = uip/msgchk.c uip/popsbr.c
uip_msgchk_LDADD = $(LDADD) $(SASLLIB)

uip_msh_SOURCES = uip/msh.c uip/mshcmds.c uip/vmhsbr.c uip/picksbr.c \
		  uip/scansbr.c uip/dropsbr.c uip/mhlsbr.c uip/termsbr.c
uip_msh_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB)

uip_new_SOURCES = uip/new.c

uip_packf_SOURCES = uip/packf.c uip/dropsbr.c

uip_pick_SOURCES = uip/pick.c uip/picksbr.c

uip_prompter_SOURCES = uip/prompter.c

uip_refile_SOURCES = uip/refile.c

uip_repl_SOURCES = uip/repl.c uip/replsbr.c uip/whatnowproc.c \
		   uip/whatnowsbr.c uip/sendsbr.c uip/annosbr.c uip/distsbr.c
uip_repl_LDADD = $(LDADD) $(ICONVLIB) $(READLINELIB)

uip_rmf_SOURCES = uip/rmf.c

uip_rmm_SOURCES = uip/rmm.c

uip_scan_SOURCES = uip/scan.c uip/scansbr.c uip/termsbr.c
uip_scan_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB)

uip_send_SOURCES = uip/send.c uip/sendsbr.c uip/annosbr.c uip/distsbr.c

uip_show_SOURCES = uip/show.c uip/mhlsbr.c uip/termsbr.c
uip_show_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB)

uip_sortm_SOURCES = uip/sortm.c

uip_whatnow_SOURCES = uip/whatnow.c uip/whatnowsbr.c uip/sendsbr.c \
		      uip/annosbr.c uip/distsbr.c
uip_whatnow_LDADD = $(LDADD) $(READLINELIB)

uip_whom_SOURCES = uip/whom.c uip/distsbr.c

##
## Programs definitions for all utilities that end up in $(libdir)
##

uip_ap_SOURCES = uip/ap.c uip/termsbr.c
uip_ap_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB)

uip_conflict_SOURCES = uip/conflict.c uip/aliasbr.c

uip_dp_SOURCES = uip/dp.c uip/termsbr.c
uip_dp_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB)

uip_fmtdump_SOURCES = uip/fmtdump.c
uip_fmtdump_LDADD = $(LDADD) $(ICONVLIB)

uip_mhl_SOURCES = uip/mhl.c uip/mhlsbr.c uip/termsbr.c
uip_mhl_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB)

uip_mhtest_SOURCES = uip/mhtest.c uip/mhparse.c uip/mhcachesbr.c \
		     uip/mhoutsbr.c uip/mhmisc.c uip/mhfree.c \
		     uip/termsbr.c uip/md5.c
uip_mhtest_LDADD = $(LDADD) $(TERMLIB)

uip_post_SOURCES = uip/post.c uip/aliasbr.c
uip_post_LDADD = mts/libmts.a $(LDADD) $(SASLLIB)

uip_rcvdist_SOURCES = uip/rcvdist.c uip/distsbr.c
uip_rcvdist_LDADD = $(LDADD) $(ICONVLIB)

uip_rcvpack_SOURCES = uip/rcvpack.c uip/dropsbr.c

uip_rcvstore_SOURCES = uip/rcvstore.c

uip_rcvtty_SOURCES = uip/rcvtty.c uip/scansbr.c uip/termsbr.c
uip_rcvtty_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB)

uip_slocal_SOURCES = uip/slocal.c uip/aliasbr.c uip/dropsbr.c
uip_slocal_LDADD = $(LDADD) $(NDBM_LIBS)

uip_spost_SOURCES = uip/spost.c uip/aliasbr.c

uip_viamail_SOURCES = uip/viamail.c uip/mhmisc.c uip/mhoutsbr.c uip/sendsbr.c \
		      uip/annosbr.c uip/distsbr.c

test_getfullname_SOURCES = test/getfullname.c
test_getfullname_LDADD = sbr/libmh.a

test_getfqdn_SOURCES = test/getfqdn.c
test_getfqdn_LDADD =

test_fakesmtp_SOURCES = test/fakesmtp.c
test_fakesmtp_LDADD =

##
## Our rebuild rules for files that aren't built via the normal mechanisms
##
config/version.c: Makefile
	sh $(srcdir)/config/version.sh $(VERSION) > $@

sbr/sigmsg.h: $(srcdir)/sbr/sigmsg.awk $(SIGNAL_H)
	$(AWK) -f $(srcdir)/sbr/sigmsg.awk $(SIGNAL_H) > $@

etc/mts.conf: $(srcdir)/etc/mts.conf.in Makefile
	rm -f $@
	$(SED) -e 's,%mts%,$(MTS),' \
	       -e 's,%mailspool%,$(mailspool),' \
	       -e 's,%etcdir%,$(sysconfdir),' \
	       -e 's,%smtpservers%,$(smtpservers),' \
	       			< $(srcdir)/etc/mts.conf.in > $@

etc/mhn.defaults: $(srcdir)/etc/mhn.defaults.sh $(MHNSEARCHPROG)
	rm -f $@
	$(srcdir)/etc/mhn.defaults.sh $(MHNSEARCHPATH) $(MHNSEARCHPROG) > $@

etc/sendfiles: $(srcdir)/etc/sendfiles.in Makefile
	rm -f $@
	$(SED) -e 's,%libdir%,$(libdir),' < $(srcdir)/etc/sendfiles.in > $@

##
## This rule gets called at "make install" time; we use it to create links
## between different programs.
##
install-exec-hook:
	rm -f $(DESTDIR)$(bindir)/flists$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/folders$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/fnext$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/fprev$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/unseen$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/prev$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/next$(EXEEXT)
	ln $(DESTDIR)$(bindir)/flist$(EXEEXT) $(DESTDIR)$(bindir)/flists$(EXEEXT)
	ln $(DESTDIR)$(bindir)/folder$(EXEEXT) $(DESTDIR)$(bindir)/folders$(EXEEXT)
	ln $(DESTDIR)$(bindir)/new$(EXEEXT) $(DESTDIR)$(bindir)/fnext$(EXEEXT)
	ln $(DESTDIR)$(bindir)/new$(EXEEXT) $(DESTDIR)$(bindir)/fprev$(EXEEXT)
	ln $(DESTDIR)$(bindir)/new$(EXEEXT) $(DESTDIR)$(bindir)/unseen$(EXEEXT)
	ln $(DESTDIR)$(bindir)/show$(EXEEXT) $(DESTDIR)$(bindir)/prev$(EXEEXT)
	ln $(DESTDIR)$(bindir)/show$(EXEEXT) $(DESTDIR)$(bindir)/next$(EXEEXT)
	if test x$(SETGID_MAIL) != x; then \
		chgrp $(MAIL_SPOOL_GRP) $(DESTDIR)$(bindir)/inc$(EXEEXT); \
		chmod 2755 $(DESTDIR)$(bindir)/inc$(EXEEXT); \
	fi

install-data-hook:
	rm -f $(DESTDIR)$(man5dir)/mh_profile.5
	ln $(DESTDIR)$(man5dir)/mh-profile.5 $(DESTDIR)$(man5dir)/mh_profile.5
##
## Make sure we remove those links at uninstall time
##
uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/flists$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/folders$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/fnext$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/fprev$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/unseen$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/prev$(EXEEXT)
	rm -f $(DESTDIR)$(bindir)/next$(EXEEXT)
	rm -f $(DESTDIR)$(man5dir)/mh_profile.5

##
## Our rules to build our internal libraries (libmh.a, libmts.a)
##
sbr_libmh_a_SOURCES = sbr/addrsbr.c sbr/ambigsw.c sbr/atooi.c sbr/brkstring.c \
		      sbr/check_charset.c sbr/client.c sbr/closefds.c \
		      sbr/concat.c sbr/context_del.c sbr/context_find.c \
		      sbr/context_foil.c sbr/context_read.c \
		      sbr/context_replace.c sbr/context_save.c \
		      sbr/copy.c sbr/copyip.c sbr/cpydata.c \
		      sbr/cpydgst.c sbr/crawl_folders.c sbr/discard.c \
		      sbr/done.c sbr/dtime.c sbr/escape_display_name.c \
		      sbr/error.c  sbr/ext_hook.c sbr/fdcompare.c \
		      sbr/folder_addmsg.c sbr/folder_delmsgs.c \
		      sbr/folder_free.c sbr/folder_pack.c \
		      sbr/folder_read.c sbr/folder_realloc.c sbr/gans.c \
		      sbr/getans.c sbr/getanswer.c sbr/getarguments.c \
		      sbr/getcpy.c sbr/getfolder.c sbr/getpass.c \
		      sbr/fmt_addr.c sbr/fmt_compile.c sbr/fmt_new.c \
		      sbr/fmt_rfc2047.c sbr/fmt_scan.c sbr/lock_file.c \
		      sbr/m_atoi.c sbr/m_backup.c sbr/m_convert.c \
		      sbr/m_draft.c sbr/m_getfld.c sbr/m_gmprot.c \
		      sbr/m_maildir.c sbr/m_name.c sbr/m_setjmp.c \
		      sbr/makedir.c sbr/mts.c sbr/norm_charmap.c sbr/path.c \
		      sbr/peekc.c sbr/pidwait.c sbr/pidstatus.c \
		      sbr/print_help.c sbr/print_sw.c sbr/print_version.c \
		      sbr/push.c sbr/putenv.c sbr/refile.c sbr/remdir.c \
		      sbr/r1bindex.c sbr/readconfig.c sbr/ruserpass.c \
		      sbr/seq_add.c sbr/seq_bits.c sbr/seq_del.c \
		      sbr/seq_getnum.c sbr/seq_list.c sbr/seq_nameok.c \
		      sbr/seq_print.c sbr/seq_read.c sbr/seq_save.c \
		      sbr/seq_setcur.c sbr/seq_setprev.c sbr/seq_setunseen.c \
		      sbr/showfile.c sbr/signals.c sbr/smatch.c \
		      sbr/snprintb.c sbr/ssequal.c sbr/strcasecmp.c \
		      sbr/strindex.c sbr/trimcpy.c sbr/uprf.c sbr/vfgets.c \
		      sbr/fmt_def.c sbr/m_msgdef.c sbr/mf.c sbr/utils.c \
		      sbr/m_mktemp.c sbr/getansreadline.c config/config.c \
		      config/version.c

sbr_libmh_a_CPPFLAGS = -I./sbr -DNMHETCDIR='"$(sysconfdir)"' \
		-DMAILSPOOL='"$(mailspool)"' \
		-DSENDMAILPATH='"$(sendmailpath)"' -DNMHBINDIR='"$(bindir)"' \
		-DNMHLIBDIR='"$(libdir)"' \
		-DDEFAULT_EDITOR='"$(editorpath)"' \
		-DDEFAULT_PAGER='"$(pagerpath)"'

sbr_libdtimep_a_SOURCES = sbr/dtimep.l
sbr_libdtimep_a_CFLAGS = $(sbr_libmh_a_CPPFLAGS) $(DISABLE_UNUSED_MACROS_WARNING)

mts_libmts_a_SOURCES = mts/smtp/hosts.c mts/smtp/smtp.c

##
## Our rules for generating man pages (both the rule for man.sed and
## the implicit rules for man page generation).
##
manext1 = 1
manext5 = 5
manext8 = 8

$(man_MANS): man/man.sed

man/man.sed: Makefile
	@echo 's,%nmhwarning%,THIS FILE HAS BEEN AUTOMATICALLY GENERATED.  DO NOT EDIT.,g' > $@
	@echo 's,%nmhversion%,nmh-$(VERSION),g' >> $@
	@echo 's,%nmhdate%,$(DATE),g' >> $@
	@echo 's,%bindir%,$(bindir),g' >> $@
	@echo 's,%etcdir%,$(sysconfdir),g' >> $@
	@echo 's,%libdir%,$(libdir),g' >> $@
	@echo 's,%mandir%,$(mandir),g' >> $@
	@echo 's,%mailspool%,$(mailspool),g' >> $@
	@echo 's,%sendmailpath%,$(sendmailpath),g' >> $@
	@echo 's,%default_editor%,$(editorpath),g' >> $@
	@echo 's,%default_pager%,$(pagerpath),g' >> $@
	@echo 's,%manext1%,$(manext1),g' >> $@
	@echo 's,%manext5%,$(manext5),g' >> $@
	@echo 's,%manext8%,$(manext8),g' >> $@
	@echo '/%mhl_forward%/r $(top_srcdir)/etc/mhl.forward' >> $@
	@echo ' s,%mhl_forward%,,g' >> $@
	@echo '/%mhl_format%/r $(top_srcdir)/etc/mhl.format' >> $@
	@echo ' s,%mhl_format%,,g' >> $@
	@echo '/%mhl_reply%/r $(top_srcdir)/etc/mhl.reply' >> $@
	@echo ' s,%mhl_reply%,,g' >> $@

man/mh-chart.man:
	@$(srcdir)/man/mh-chart-gen.sh > $@

.man.$(manext1):
	@$(SED) -f man/man.sed $< > $@

.man.$(manext5):
	@$(SED) -f man/man.sed $< > $@

.man.$(manext8):
	@$(SED) -f man/man.sed $< > $@

## Don't include commit hashes in ChangeLog.
ChangeLog:
	@[ -d .git ]  &&  git --no-pager log --abbrev-commit | \
           egrep -v '^commit [0-9a-f]+$$' > $@  ||  true
## Make ChangeLog phony so it will always get regenerated.  But don't
## fail if we don't have a git repository in order to allow
## regeneration of distribution archive or RPM.
.PHONY: ChangeLog

##
## Our RPM build target
##
rpm: dist
	@test -d $(rpmdir)/SOURCES || $(MKDIR_P) $(rpmdir)/SOURCES
	@test -d $(rpmdir)/SPECS || $(MKDIR_P) $(rpmdir)/SPECS
	@mv -f $(DIST_ARCHIVES) $(rpmdir)/SOURCES
	@cp -p VERSION $(rpmdir)/SOURCES
	@cp -p nmh.spec $(rpmdir)/SPECS
	@rpmbuild --define '_topdir $(rpmdir)' \
	  --define '_tmppath %{_topdir}/TMP' \
	  --define "_sysconfdir $(DESTDIR)$(sysconfdir)" \
	  -ba $(rpmdir)/SPECS/nmh.spec
.PHONY: rpm

##
## A target to build information needed by cscope
##
cscope:
	echo "-I $(srcdir)/h -I $(srcdir)/sbr -I $(srcdir)/uip -I $(srcdir)/mts/smtp" > cscope.files
	find $(srcdir) \( -name \*.c -o -name \*.l \) -print | grep -v dtimep.c >> cscope.files
.PHONY: cscope


##
## Use GNU gcov to find the coverage of the test suite.
## These targets are not integrated into the other automake
## targets because they will be used so infrequently, if at all.
##
gcov:
	@if ./config.status --config | grep -e --enable-debug >/dev/null; then \
	   echo For best results with gcov, configure without --enable-debug; \
	 fi
	@echo rebuilding with AM_CFLAGS=--coverage . . .
	@(make clean  &&  make AM_CFLAGS=--coverage) > /dev/null  &&  make check
	@for i in `find . -name '*.gcda'`; do \
	   gcov -p -o `echo $$i | $(SED) 's%\\(.*\\)/%\\1 %'`; \
	 done
	@for i in `find . -name '*.gcno'`; do \
	   if ! test -f `echo $$i | sed 's%\.gcno%.gcda%'`; then \
	     echo untested: $$i; \
	   fi; \
	 done
gcov-mostlyclean:
	@find . -name '*.gcno' -o -name '*.gcda' | xargs rm
gcov-clean: gcov-mostlyclean
	@find . -name '*.gcov' | xargs rm
.PHONY: gcov gcov-mostlyclean gcov-clean


##
## Rules to upload the distribution to savannah
##
upload: dist
	@if test -z "$(SAVANNAH_USERNAME)"; then \
		echo "You need to set SAVANNAH_USERNAME to the your username on savannah.gnu.org." ;\
		echo "You can set it either in your environment or on the make command line."; \
		false; \
	fi
	@echo "Did you run 'make distcheck' already?"
	@if test -z "$(SKIP_GPG_SIG)"; then \
		echo "Creating GPG signature (set SKIP_GPG_SIG to bypass this step)"; \
		gpg --output $(DIST_ARCHIVES).sig --detatch-sig $(DIST_ARCHIVES); \
	fi
	@echo "Setting permissions for savannah.gnu.org"
	chmod 664 $(DIST_ARCHIVES)
	if test -f $(DIST_ARCHIVES).sig; then \
		chmod 664 $(DIST_ARCHIVES).sig; \
	fi
	@echo "Uploading to savannah"
	scp -p $(DIST_ARCHIVES)* $(SAVANNAH_USERNAME)@savannah.gnu.org:/releases/nmh
.PHONY: upload
