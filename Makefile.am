## Our Makefile.am template for Makefile.in (and, eventually, Makefile)
##
## Process this with automake to generate Makefile.in
## Lists are in `LC_ALL=C sort' order.
##

## We set this to get our autoconf macros in the m4 directory
ACLOCAL_AMFLAGS = -I m4

AM_YFLAGS = -d

##
## This is the default set of libraries all programs link against.  Some
## programs add extra libraries to this set, so they override this with
## a program-specific LDADD variable.  Note that in all of the cases today
## LDADD is included in the program-specific LDADD since we want to add
## to this list of libraries, not replace it.
##
LDADD = sbr/libmh.a

##
## These are used to create the default mhn.defaults config file
## MHNSEARCHPATH is the default path to look for MIME content handlers
## MHNSEARCHPROG is the script used to find the support programs
##
MHNSEARCHPATH = "$(PATH)"
MHNSEARCHPROG = $(srcdir)/etc/mhn.find.sh

##
## Install config files and back-end programs in our own subdirecctories.
##
nmhetcdir  = @sysconfdir@/nmh
nmhlibexecdir = @libexecdir@/nmh

##
## nmh _does_ have a test suite!
##
TESTS_ENVIRONMENT = MH_OBJ_DIR="@abs_builddir@" \
		    MH_VERSION="$(VERSION)" \
		    OAUTH_SUPPORT='@OAUTH_SUPPORT@' \
		    CURL_USER_AGENT='@CURL_USER_AGENT@' \
		    MH_TEST_DIR=`cd "@abs_builddir@" && pwd -P`/test/testdir \
		    nmhlibexecdir="$(nmhlibexecdir)" bindir="$(bindir)" \
		    mandir="$(mandir)" nmhetcdir="$(nmhetcdir)" \
		    nmhetcdirinst="@nmhetcdirinst@$(nmhetcdir)" \
		    supported_locks="$(supported_locks)" \
		    default_locking="${default_locking}" \
		    MULTIBYTE_ENABLED=$(MULTIBYTE_ENABLED) \
		    ICONV_ENABLED=$(ICONV_ENABLED) \
		    $(TESTS_SHELL) ## Keep at end of TESTS_ENVIRONMENT.

##
## Important note: the "cleanup" test should always be last
##
TESTS = \
    test/ali/test-ali \
    test/anno/test-anno \
    test/bad-input/test-header \
    test/burst/test-burst \
    test/burst/test-burst-mime \
    test/comp/test-comp-format \
    test/dist/test-dist \
    test/folder/test-coverage \
    test/folder/test-create \
    test/folder/test-nocreate \
    test/folder/test-packf \
    test/folder/test-recurse \
    test/folder/test-sortm \
    test/folder/test-total \
    test/format/test-ap \
    test/format/test-curses \
    test/format/test-dp \
    test/format/test-fmtdump \
    test/format/test-functions \
    test/format/test-localmbox \
    test/format/test-myhost \
    test/format/test-mymbox \
    test/format/test-myname \
    test/format/test-nullstr \
    test/format/test-rightjustify \
    test/format/test-unquote \
    test/forw/test-forw-coverage \
    test/forw/test-forw-digest \
    test/forw/test-forw-format \
    test/inc/test-deb359167 \
    test/inc/test-eom-align \
    test/inc/test-inc-scanout \
    test/inc/test-msgchk \
    test/inc/test-pop \
    test/install-mh/test-install-mh \
    test/install-mh/test-version-check \
    test/locking/test-datalocking \
    test/locking/test-spoollocking \
    test/manpages/test-manpages \
    test/mhbuild/test-attach \
    test/mhbuild/test-cte \
    test/mhbuild/test-ext-params \
    test/mhbuild/test-forw \
    test/mhbuild/test-header-encode \
    test/mhbuild/test-mhbuild \
    test/mhbuild/test-utf8-body \
    test/mhfixmsg/test-mhfixmsg \
    test/mhical/test-mhical \
    test/mhl/test-mhl-flags \
    test/mhl/test-rfc6532 \
    test/mhlist/test-ext-params \
    test/mhlist/test-mhlist \
    test/mhmail/test-mhmail \
    test/mhparam/test-mhparam \
    test/mhpath/test-mhpath \
    test/mhshow/test-binary \
    test/mhshow/test-charset \
    test/mhshow/test-cte-binary \
    test/mhshow/test-msg-buffer-boundaries \
    test/mhshow/test-qp \
    test/mhshow/test-subpart \
    test/mhshow/test-textcharset \
    test/mhstore/test-mhstore \
    test/mkstemp/test-mkstemp \
    test/new/test-basic \
    test/oauth/test-inc \
    test/oauth/test-mhlogin \
    test/oauth/test-mhparam \
    test/oauth/test-send \
    test/oauth/test-sendfrom \
    test/oauth/test-share \
    test/pick/test-pick \
    test/pick/test-stderr \
    test/post/test-messageid \
    test/post/test-mts \
    test/post/test-post-aliases \
    test/post/test-post-basic \
    test/post/test-post-bcc \
    test/post/test-post-dcc \
    test/post/test-post-envelope \
    test/post/test-post-fcc \
    test/post/test-post-group \
    test/post/test-post-multifrom \
    test/post/test-post-multiple \
    test/post/test-rfc6531 \
    test/post/test-sendfiles \
    test/prompter/test-prompter \
    test/rcv/test-rcvdist \
    test/rcv/test-rcvpack \
    test/rcv/test-rcvstore \
    test/rcv/test-rcvtty \
    test/refile/test-refile \
    test/repl/test-convert \
    test/repl/test-if-str \
    test/repl/test-multicomp \
    test/repl/test-repl \
    test/repl/test-trailing-newline \
    test/scan/test-header-parsing \
    test/scan/test-scan \
    test/scan/test-scan-multibyte \
    test/send/test-sendfrom \
    test/sequences/test-flist \
    test/sequences/test-mark \
    test/sequences/test-out-of-range \
    test/show/test-show \
    test/slocal/test-slocal \
    test/whatnow/test-attach-detach \
    test/whatnow/test-cd \
    test/whatnow/test-ls \
    test/whom/test-whom \
    test/cleanup ## The "cleanup" test should always be last.

XFAIL_TESTS =

check_SCRIPTS = test/common.sh

check_PROGRAMS = \
    test/fakehttp \
    test/fakepop \
    test/fakesmtp \
    test/getcanon \
    test/getcwidth \
    test/getfullname \
    test/runpty \
    #

## So they can be built without builing the `check' target.
check-programs: $(check_PROGRAMS)

## The location of installed nmhetcdir is, for all purposes except
## distcheck, $nmhetcdir.  For distcheck, prepend $MH_INST_DIR (from
## test/common.sh.in), which is based on $MH_TEST_DIR (from
## $TESTS_ENVIRONMENT).
AM_DISTCHECK_CONFIGURE_FLAGS = DISABLE_SETGID_MAIL=1 \
			NMHETCDIRINST='$${abs_builddir}/test/testdir/inst'

##
## Stuff that should be cleaned via "make clean"
## automake 1.12.6 on FreeBSD 9 needs the sbr/dtimep.c.
##
CLEANFILES = \
    $(man_MANS) \
    *.plist \
    config/version.c \
    etc/bash_completion_nmh \
    etc/mhn.defaults \
    etc/mts.conf \
    man/man.sed \
    man/mh-chart.man \
    sbr/dtimep.c \
    #

clean-local:
	@rm -rf RPM a.out.dSYM uip/a.out.dSYM
	@rm -rf test/testdir

##
## Stuff that should be cleaned via "make maintainer-clean"
##
MAINTAINERCLEANFILES = \
    *.plist \
    atags \
    cscope.files \
    cscope.in.out \
    cscope.out \
    cscope.po.out \
    #

##
## And our own superclean, to get everything left by maintainer-clean.
##
SUPERCLEANFILES = \
    ChangeLog \
    Makefile.in \
    aclocal.m4 \
    compile \
    config.guess \
    config.h.in \
    config.h.in~ \
    config.sub \
    configure \
    depcomp \
    install-sh \
    missing \
    nmh-*.tar.gz \
    nmh-*.tar.gz.sig \
    test-driver \
    ylwrap \
    #

superclean: maintainer-clean
	@rm -f $(SUPERCLEANFILES)
.PHONY: superclean

##
## This is a list of all programs that get installed in the "bin" directory
## Note that primary difference between "PROGRAMS" and "SCRIPTS" is that
## the latter do not have $(EXEEXT) added on the end.
##
bin_PROGRAMS = \
    uip/ali \
    uip/anno \
    uip/burst \
    uip/comp \
    uip/dist \
    uip/flist \
    uip/fmttest \
    uip/folder \
    uip/forw \
    uip/inc \
    uip/install-mh \
    uip/mark \
    uip/mhbuild \
    uip/mhfixmsg \
    uip/mhical \
    uip/mhlist \
    uip/mhlogin \
    uip/mhn \
    uip/mhparam \
    uip/mhpath \
    uip/mhshow \
    uip/mhstore \
    uip/msgchk \
    uip/new \
    uip/packf \
    uip/pick \
    uip/prompter \
    uip/refile \
    uip/repl \
    uip/rmf \
    uip/rmm \
    uip/scan \
    uip/send \
    uip/show \
    uip/sortm \
    uip/whatnow \
    uip/whom \
    #

noinst_PROGRAMS = \
    uip/imaptest \
    #

bin_SCRIPTS = \
    etc/sendfiles \
    uip/mhmail \
    #

##
## This is all programs that get installed in the "libexec/nmh" directory
##
nmhlibexec_PROGRAMS = \
    uip/ap \
    uip/dp \
    uip/fmtdump \
    uip/mhl \
    uip/mkstemp \
    uip/post \
    uip/rcvdist \
    uip/rcvpack \
    uip/rcvstore \
    uip/rcvtty \
    uip/slocal \
    uip/viamail \
    #

nmhlibexec_SCRIPTS = uip/spost

##
## Internal libraries that we create as part of the build process
## but do not install
##
noinst_LIBRARIES = \
    mts/libmts.a \
    sbr/libmh.a \
    #

##
## These are all of our header files.  Right now we don't install any of
## them, but that might change in the future.
##
noinst_HEADERS = \
    h/addrsbr.h \
    h/aliasbr.h \
    h/crawl_folders.h \
    h/done.h \
    h/dropsbr.h \
    h/fmt_compile.h \
    h/fmt_scan.h \
    h/icalendar.h \
    h/md5.h \
    h/mf.h \
    h/mh.h \
    h/mhcachesbr.h \
    h/mhparse.h \
    h/mime.h \
    h/mts.h \
    h/netsec.h \
    h/nmh.h \
    h/oauth.h \
    h/picksbr.h \
    h/popsbr.h \
    h/prototypes.h \
    h/rcvmail.h \
    h/scansbr.h \
    h/signals.h \
    h/tws.h \
    h/utils.h \
    mts/smtp/smtp.h \
    sbr/ambigsw.h \
    sbr/arglist.h \
    sbr/atooi.h \
    sbr/base64.h \
    sbr/brkstring.h \
    sbr/context_del.h \
    sbr/context_find.h \
    sbr/context_foil.h \
    sbr/context_read.h \
    sbr/context_replace.h \
    sbr/context_save.h \
    sbr/ctype-checked.h \
    sbr/error.h \
    sbr/lock_file.h \
    sbr/m_maildir.h \
    sbr/m_mktemp.h \
    sbr/m_popen.h \
    sbr/m_rand.h \
    sbr/makedir.h \
    sbr/message_id.h \
    sbr/mime_type.h \
    sbr/path.h \
    sbr/pidstatus.h \
    sbr/print_help.h \
    sbr/print_sw.h \
    sbr/print_version.h \
    sbr/push.h \
    sbr/read_line.h \
    sbr/readconfig.h \
    sbr/seq_add.h \
    sbr/seq_bits.h \
    sbr/seq_del.h \
    sbr/seq_getnum.h \
    sbr/seq_list.h \
    sbr/seq_nameok.h \
    sbr/seq_print.h \
    sbr/terminal.h \
    sbr/unquote.h \
    sbr/vector.h \
    thirdparty/jsmn/jsmn.h \
    uip/forwsbr.h \
    uip/mhfree.h \
    uip/mhmisc.h \
    uip/mhoutsbr.h \
    uip/mhshowsbr.h \
    uip/replsbr.h \
    #

##
## Extra files we need to install in various places
##
dist_nmhetc_DATA = \
    etc/MailAliases \
    etc/components \
    etc/digestcomps \
    etc/distcomps \
    etc/forwcomps \
    etc/mhical.12hour \
    etc/mhical.24hour \
    etc/mhl.body \
    etc/mhl.digest \
    etc/mhl.format \
    etc/mhl.forward \
    etc/mhl.headers \
    etc/mhl.reply \
    etc/mhl.replywithoutbody \
    etc/mhshow.marker \
    etc/rcvdistcomps \
    etc/rcvdistcomps.outbox \
    etc/replcomps \
    etc/replgroupcomps \
    etc/rmmproc.messageid \
    etc/scan.MMDDYY \
    etc/scan.YYYYMMDD \
    etc/scan.curses \
    etc/scan.default \
    etc/scan.highlighted \
    etc/scan.mailx \
    etc/scan.nomime \
    etc/scan.size \
    etc/scan.time \
    etc/scan.timely \
    etc/scan.unseen \
    #

##
## The same as above, but we don't include these in the distribution
## (because they're generated at compile time)
##
nmhetc_DATA = \
    etc/bash_completion_nmh \
    etc/mhn.defaults \
    etc/mts.conf \
    #

##
## Documentation that gets installed in docdir
##
dist_doc_DATA = \
    COPYRIGHT \
    INSTALL \
    NEWS \
    README \
    VERSION \
    docs/COMPLETION-TCSH \
    docs/COMPLETION-ZSH \
    docs/DIFFERENCES \
    docs/FAQ \
    docs/MAIL.FILTERING \
    docs/MAILING-LISTS \
    docs/README-ATTACHMENTS \
    docs/README-HOOKS \
    docs/README-components \
    docs/README.SASL \
    docs/README.about \
    docs/README.developers \
    docs/README.manpages \
    docs/TODO \
    #

##
## Contribs that get installed in docdir/contrib/
##
docs_contribdir = $(docdir)/contrib
dist_docs_contrib_SCRIPTS = \
    docs/contrib/localpostproc \
    docs/contrib/ml \
    docs/contrib/replaliases \
    docs/contrib/replyfilter \
    docs/contrib/vpick \
    #

dist_docs_contrib_DATA = docs/contrib/replaliases

##
## Our man pages
##
man_MANS = \
    man/MH.7 \
    man/ali.1 \
    man/anno.1 \
    man/ap.8 \
    man/burst.1 \
    man/comp.1 \
    man/dist.1 \
    man/dp.8 \
    man/flist.1 \
    man/flists.1 \
    man/fmtdump.8 \
    man/fmttest.1 \
    man/fnext.1 \
    man/folder.1 \
    man/folders.1 \
    man/forw.1 \
    man/fprev.1 \
    man/inc.1 \
    man/install-mh.1 \
    man/mark.1 \
    man/mh-alias.5 \
    man/mh-chart.7 \
    man/mh-draft.5 \
    man/mh-folders.5 \
    man/mh-format.5 \
    man/mh-mail.5 \
    man/mh-mime.7 \
    man/mh-mkstemp.1 \
    man/mh-profile.5 \
    man/mh-sequence.5 \
    man/mh-tailor.5 \
    man/mh_profile.5 \
    man/mhbuild.1 \
    man/mhfixmsg.1 \
    man/mhical.1 \
    man/mhl.1 \
    man/mhlist.1 \
    man/mhlogin.1 \
    man/mhmail.1 \
    man/mhn.1 \
    man/mhparam.1 \
    man/mhpath.1 \
    man/mhshow.1 \
    man/mhstore.1 \
    man/msgchk.1 \
    man/mts.conf.5 \
    man/new.1 \
    man/next.1 \
    man/nmh.7 \
    man/packf.1 \
    man/pick.1 \
    man/post.8 \
    man/prev.1 \
    man/prompter.1 \
    man/rcvdist.1 \
    man/rcvpack.1 \
    man/rcvstore.1 \
    man/rcvtty.1 \
    man/refile.1 \
    man/repl.1 \
    man/rmf.1 \
    man/rmm.1 \
    man/scan.1 \
    man/send.1 \
    man/sendfiles.1 \
    man/show.1 \
    man/slocal.1 \
    man/sortm.1 \
    man/unseen.1 \
    man/whatnow.1 \
    man/whom.1 \
    #

##
## Sources for our man pages
##
man_SRCS = \
    man/MH.man \
    man/ali.man \
    man/anno.man \
    man/ap.man \
    man/burst.man \
    man/comp.man \
    man/dist.man \
    man/dp.man \
    man/flist.man \
    man/flists.man \
    man/fmtdump.man \
    man/fmttest.man \
    man/fnext.man \
    man/folder.man \
    man/folders.man \
    man/forw.man \
    man/fprev.man \
    man/inc.man \
    man/install-mh.man \
    man/mark.man \
    man/mh-alias.man \
    man/mh-chart-gen.sh \
    man/mh-draft.man \
    man/mh-folders.man \
    man/mh-format.man \
    man/mh-mail.man \
    man/mh-mime.man \
    man/mh-mkstemp.man \
    man/mh-profile.man \
    man/mh-sequence.man \
    man/mh-tailor.man \
    man/mh_profile.man \
    man/mhbuild.man \
    man/mhfixmsg.man \
    man/mhical.man \
    man/mhl.man \
    man/mhlist.man \
    man/mhlogin.man \
    man/mhmail.man \
    man/mhn.man \
    man/mhparam.man \
    man/mhpath.man \
    man/mhshow.man \
    man/mhstore.man \
    man/msgchk.man \
    man/mts.conf.man \
    man/new.man \
    man/next.man \
    man/nmh.man \
    man/packf.man \
    man/pick.man \
    man/post.man \
    man/prev.man \
    man/prompter.man \
    man/rcvdist.man \
    man/rcvpack.man \
    man/rcvstore.man \
    man/rcvtty.man \
    man/refile.man \
    man/repl.man \
    man/rmf.man \
    man/rmm.man \
    man/scan.man \
    man/send.man \
    man/sendfiles.man \
    man/show.man \
    man/slocal.man \
    man/sortm.man \
    man/unseen.man \
    man/whatnow.man \
    man/whom.man \
    #

##
## Files we need to include in the distribution which aren't found by
## Automake using the automatic rules
##
EXTRA_DIST = \
    $(MHNSEARCHPROG) \
    $(TESTS) \
    $(man_SRCS) \
    DATE \
    MACHINES \
    SPECS/nmh.cygport \
    SPECS/nmh.spec \
    autogen.sh \
    build_nmh \
    config/version.sh \
    docs/ChangeLog_MH-3_to_MH-6.6 \
    docs/ChangeLog_MH-6.7.0_to_MH-6.8.4.html \
    etc/bash_completion_nmh-gen \
    etc/mhn.defaults.sh \
    etc/mts.conf.in \
    etc/sendfiles \
    sbr/icalparse.h \
    test/README \
    test/fakesendmail \
    test/inc/deb359167.mbox \
    test/inc/filler.txt \
    test/inc/fromline.txt \
    test/inc/md5sums \
    test/inc/msgheader.txt \
    test/mhbuild/nulls \
    test/mhbuild/somebinary \
    test/mhbuild/textplain \
    test/mhbuild/tiny.jpg \
    test/mhmail/attachment.txt \
    test/oauth/common.sh \
    test/post/test-post-common.sh \
    test/send/README \
    test/valgrind.supp \
    uip/mhmail \
    #

##
## These are all of the definitions for each of the programs listed above.
## They describe the source files that make up the programs and any particular
## overrides that are used to select different library options from the
## default.
##
POSTLINK = @POSTLINK@

uip_ali_SOURCES = uip/ali.c uip/aliasbr.c
uip_ali_LDADD = $(LDADD) $(POSTLINK)

uip_anno_SOURCES = uip/anno.c uip/annosbr.c
uip_anno_LDADD = $(LDADD) $(POSTLINK)

uip_burst_SOURCES = uip/burst.c uip/mhparse.c uip/mhmisc.c uip/mhfree.c \
		    uip/mhcachesbr.c
uip_burst_LDADD = $(LDADD) $(ICONVLIB) $(POSTLINK)

uip_comp_SOURCES = uip/comp.c uip/whatnowproc.c uip/whatnowsbr.c uip/sendsbr.c \
		   uip/annosbr.c uip/distsbr.c
uip_comp_LDADD = $(LDADD) $(READLINELIB) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_dist_SOURCES = uip/dist.c uip/whatnowproc.c uip/whatnowsbr.c uip/sendsbr.c \
		   uip/annosbr.c uip/distsbr.c uip/forwsbr.c
uip_dist_LDADD = $(LDADD) $(READLINELIB) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_flist_SOURCES = uip/flist.c
uip_flist_LDADD = $(LDADD) $(POSTLINK)

uip_folder_SOURCES = uip/folder.c
uip_folder_LDADD = $(LDADD) $(POSTLINK)

uip_forw_SOURCES = uip/forw.c uip/whatnowproc.c uip/whatnowsbr.c uip/sendsbr.c \
		   uip/annosbr.c uip/distsbr.c uip/forwsbr.c
uip_forw_LDADD = $(LDADD) $(READLINELIB) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_imaptest_SOURCES = uip/imaptest.c
uip_imaptest_LDADD = $(LDADD) $(SASLLIB) $(CURLLIB) $(TLSLIB) $(POSTLINK)

uip_inc_SOURCES = uip/inc.c uip/scansbr.c uip/dropsbr.c uip/popsbr.c
uip_inc_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(SASLLIB) $(CURLLIB) \
		$(TLSLIB) $(POSTLINK)

uip_install_mh_SOURCES = uip/install-mh.c
uip_install_mh_LDADD = $(LDADD) $(POSTLINK)

uip_mark_SOURCES = uip/mark.c
uip_mark_LDADD = $(LDADD) $(POSTLINK)

uip_mhbuild_SOURCES = uip/mhbuild.c uip/mhbuildsbr.c uip/mhcachesbr.c \
		      uip/mhlistsbr.c uip/mhoutsbr.c uip/mhmisc.c  \
		      uip/mhfree.c uip/mhparse.c \
		      uip/mhstoresbr.c uip/mhshowsbr.c
uip_mhbuild_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_mhfixmsg_SOURCES = uip/mhfixmsg.c uip/mhparse.c uip/mhcachesbr.c \
		       uip/mhoutsbr.c uip/mhmisc.c uip/mhfree.c \
		       uip/mhshowsbr.c uip/mhlistsbr.c
uip_mhfixmsg_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_mhical_SOURCES = uip/mhical.c
uip_mhical_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_mhlist_SOURCES = uip/mhlist.c uip/mhparse.c uip/mhcachesbr.c \
		     uip/mhlistsbr.c uip/mhmisc.c uip/mhfree.c
uip_mhlist_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_mhn_SOURCES = uip/mhn.c uip/mhparse.c uip/mhcachesbr.c uip/mhshowsbr.c \
		  uip/mhlistsbr.c uip/mhstoresbr.c uip/mhmisc.c uip/mhfree.c
uip_mhn_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_mhparam_SOURCES = uip/mhparam.c
uip_mhparam_LDADD = $(LDADD) $(POSTLINK)

uip_mhpath_SOURCES = uip/mhpath.c
uip_mhpath_LDADD = $(LDADD) $(POSTLINK)

uip_mhshow_SOURCES = uip/mhshow.c uip/mhparse.c uip/mhcachesbr.c \
		     uip/mhshowsbr.c uip/mhlistsbr.c uip/mhmisc.c \
		     uip/mhfree.c
uip_mhshow_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_mhstore_SOURCES = uip/mhstore.c uip/mhparse.c uip/mhcachesbr.c \
		      uip/mhshowsbr.c uip/mhlistsbr.c uip/mhstoresbr.c \
		      uip/mhmisc.c uip/mhfree.c
uip_mhstore_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_msgchk_SOURCES = uip/msgchk.c uip/popsbr.c
uip_msgchk_LDADD = $(LDADD) $(SASLLIB) $(CURLLIB) $(TLSLIB) $(POSTLINK)

uip_new_SOURCES = uip/new.c
uip_new_LDADD = $(LDADD) $(POSTLINK)

uip_packf_SOURCES = uip/packf.c uip/dropsbr.c
uip_packf_LDADD = $(LDADD) $(POSTLINK)

uip_pick_SOURCES = uip/pick.c uip/picksbr.c
uip_pick_LDADD = $(LDADD) $(ICONVLIB) $(POSTLINK)

uip_prompter_SOURCES = uip/prompter.c
uip_prompter_LDADD = $(LDADD) $(POSTLINK)

uip_refile_SOURCES = uip/refile.c
uip_refile_LDADD = $(LDADD) $(POSTLINK)

uip_repl_SOURCES = uip/repl.c uip/replsbr.c uip/whatnowproc.c uip/whatnowsbr.c \
		   uip/sendsbr.c uip/annosbr.c uip/distsbr.c
uip_repl_LDADD = $(LDADD) $(READLINELIB) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_rmf_SOURCES = uip/rmf.c
uip_rmf_LDADD = $(LDADD) $(POSTLINK)

uip_rmm_SOURCES = uip/rmm.c
uip_rmm_LDADD = $(LDADD) $(POSTLINK)

uip_scan_SOURCES = uip/scan.c uip/scansbr.c
uip_scan_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_send_SOURCES = uip/send.c uip/sendsbr.c uip/annosbr.c \
		   uip/distsbr.c
uip_send_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_show_SOURCES = uip/show.c uip/mhlsbr.c
uip_show_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_sortm_SOURCES = uip/sortm.c
uip_sortm_LDADD = $(LDADD) $(POSTLINK)

uip_whatnow_SOURCES = uip/whatnow.c uip/whatnowsbr.c uip/sendsbr.c \
		      uip/annosbr.c uip/distsbr.c
uip_whatnow_LDADD = $(LDADD) $(READLINELIB) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_whom_SOURCES = uip/whom.c uip/distsbr.c
uip_whom_LDADD = $(LDADD) $(POSTLINK)

##
## Program definitions for all utilities that end up in $(nmhlibexecdir)
##

uip_ap_SOURCES = uip/ap.c
uip_ap_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_dp_SOURCES = uip/dp.c
uip_dp_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_fmtdump_SOURCES = uip/fmtdump.c
uip_fmtdump_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_fmttest_SOURCES = uip/fmttest.c
uip_fmttest_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_mhl_SOURCES = uip/mhl.c uip/mhlsbr.c
uip_mhl_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_mhlogin_SOURCES = uip/mhlogin.c
uip_mhlogin_LDADD = mts/libmts.a $(LDADD) $(CURLLIB) $(POSTLINK)

uip_mkstemp_SOURCES = uip/mkstemp.c
uip_mkstemp_LDADD = $(LDADD) $(POSTLINK)

uip_post_SOURCES = uip/post.c uip/aliasbr.c
uip_post_LDADD = mts/libmts.a $(LDADD) $(SASLLIB) $(CURLLIB) $(TLSLIB) \
		 $(POSTLINK)

uip_rcvdist_SOURCES = uip/rcvdist.c uip/distsbr.c
uip_rcvdist_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_rcvpack_SOURCES = uip/rcvpack.c uip/dropsbr.c
uip_rcvpack_LDADD = $(LDADD) $(POSTLINK)

uip_rcvstore_SOURCES = uip/rcvstore.c
uip_rcvstore_LDADD = $(LDADD) $(POSTLINK)

uip_rcvtty_SOURCES = uip/rcvtty.c uip/scansbr.c
uip_rcvtty_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

uip_slocal_SOURCES = uip/slocal.c uip/aliasbr.c uip/dropsbr.c
uip_slocal_LDADD = $(LDADD) $(NDBM_LIBS) $(POSTLINK)

uip_viamail_SOURCES = uip/viamail.c uip/mhmisc.c uip/sendsbr.c \
		      uip/annosbr.c uip/distsbr.c
uip_viamail_LDADD = $(LDADD) $(TERMLIB) $(ICONVLIB) $(POSTLINK)

##
## Other program definitions
##

test_getfullname_SOURCES = test/getfullname.c
test_getfullname_LDADD = $(LDADD) $(POSTLINK)

test_getcanon_SOURCES = test/getcanon.c
test_getcanon_LDADD = $(POSTLINK)

test_fakepop_SOURCES = test/fakepop.c test/server.c
test_fakepop_LDADD = $(POSTLINK)

test_fakesmtp_SOURCES = test/fakesmtp.c test/server.c
test_fakesmtp_LDADD = $(POSTLINK)

test_fakehttp_SOURCES = test/fakehttp.c test/server.c
test_fakehttp_LDADD = $(LDADD) $(POSTLINK)

test_getcwidth_SOURCES = test/getcwidth.c
test_getcwidth_LDADD = $(POSTLINK)

test_runpty_SOURCES = test/runpty.c
test_runpty_LDADD = $(POSTLINK)

##
## Our rebuild rules for files that aren't built via the normal mechanisms
##

## Hard-code the config/version.c target, instead of using $@, so this
## rule works for the distcheck target with Solaris (System V) make.
## distcheck uses VPATH, causes that make to prepend the VPATH to $@.
config/version.c: Makefile $(srcdir)/config/version.sh
	env srcdir="$(srcdir)" sh $(srcdir)/config/version.sh $(VERSION) > ./config/version.c

etc/bash_completion_nmh: $(srcdir)/etc/bash_completion_nmh-gen
	$(srcdir)/etc/bash_completion_nmh-gen > $@

etc/mts.conf: $(srcdir)/etc/mts.conf.in Makefile
	@rm -f $@
	$(SED) -e 's,%mts%,$(MTS),' \
	       -e 's,%mailspool%,$(mailspool),' \
	       -e 's,%smtpserver%,$(smtpserver),' \
	       -e 's,%default_locking%,$(default_locking),' \
	       -e 's,%supported_locks%,$(supported_locks),' \
				< $(srcdir)/etc/mts.conf.in > $@

etc/mhn.defaults: $(srcdir)/etc/mhn.defaults.sh $(MHNSEARCHPROG)
	@rm -f $@
	$(srcdir)/etc/mhn.defaults.sh $(MHNSEARCHPATH) $(MHNSEARCHPROG) > $@

##
## Use configure to set up default LN, but through a make variable so
## that it can be overridden at install time.
##
LN = @LN@

##
## This rule gets called at "make install" time; we use it to create links
## between different programs.
##
install-exec-hook:
	@rm -f $(DESTDIR)$(bindir)/flists$(EXEEXT)
	@rm -f $(DESTDIR)$(bindir)/folders$(EXEEXT)
	@rm -f $(DESTDIR)$(bindir)/fnext$(EXEEXT)
	@rm -f $(DESTDIR)$(bindir)/fprev$(EXEEXT)
	@rm -f $(DESTDIR)$(bindir)/unseen$(EXEEXT)
	@rm -f $(DESTDIR)$(bindir)/prev$(EXEEXT)
	@rm -f $(DESTDIR)$(bindir)/next$(EXEEXT)
	$(LN) $(DESTDIR)$(bindir)/flist$(EXEEXT) $(DESTDIR)$(bindir)/flists$(EXEEXT)
	$(LN) $(DESTDIR)$(bindir)/folder$(EXEEXT) $(DESTDIR)$(bindir)/folders$(EXEEXT)
	$(LN) $(DESTDIR)$(bindir)/new$(EXEEXT) $(DESTDIR)$(bindir)/fnext$(EXEEXT)
	$(LN) $(DESTDIR)$(bindir)/new$(EXEEXT) $(DESTDIR)$(bindir)/fprev$(EXEEXT)
	$(LN) $(DESTDIR)$(bindir)/new$(EXEEXT) $(DESTDIR)$(bindir)/unseen$(EXEEXT)
	$(LN) $(DESTDIR)$(bindir)/show$(EXEEXT) $(DESTDIR)$(bindir)/prev$(EXEEXT)
	$(LN) $(DESTDIR)$(bindir)/show$(EXEEXT) $(DESTDIR)$(bindir)/next$(EXEEXT)
	if test x$(SETGID_MAIL) != x; then \
		chgrp $(MAIL_SPOOL_GRP) $(DESTDIR)$(bindir)/inc$(EXEEXT) && \
		chmod 2755 $(DESTDIR)$(bindir)/inc$(EXEEXT); \
	fi

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

##
## Our rules to build our internal libraries (libmh.a, libmts.a)
##
sbr_libmh_a_SOURCES = \
    config/config.c \
    config/version.c \
    sbr/addrsbr.c \
    sbr/ambigsw.c \
    sbr/arglist.c \
    sbr/atooi.c \
    sbr/base64.c \
    sbr/brkstring.c \
    sbr/charstring.c \
    sbr/check_charset.c \
    sbr/client.c \
    sbr/closefds.c \
    sbr/concat.c \
    sbr/context_del.c \
    sbr/context_find.c \
    sbr/context_foil.c \
    sbr/context_read.c \
    sbr/context_replace.c \
    sbr/context_save.c \
    sbr/copyip.c \
    sbr/cpydata.c \
    sbr/cpydgst.c \
    sbr/crawl_folders.c \
    sbr/credentials.c \
    sbr/ctype-checked.c \
    sbr/datetime.c \
    sbr/discard.c \
    sbr/done.c \
    sbr/dtime.c \
    sbr/dtimep.l \
    sbr/encode_rfc2047.c \
    sbr/error.c \
    sbr/escape_addresses.c \
    sbr/ext_hook.c \
    sbr/fdcompare.c \
    sbr/fmt_addr.c \
    sbr/fmt_compile.c \
    sbr/fmt_new.c \
    sbr/fmt_rfc2047.c \
    sbr/fmt_scan.c \
    sbr/folder_addmsg.c \
    sbr/folder_delmsgs.c \
    sbr/folder_free.c \
    sbr/folder_pack.c \
    sbr/folder_read.c \
    sbr/folder_realloc.c \
    sbr/getarguments.c \
    sbr/getcpy.c \
    sbr/geteditor.c \
    sbr/getfolder.c \
    sbr/getpass.c \
    sbr/icalendar.l \
    sbr/icalparse.y \
    sbr/lock_file.c \
    sbr/m_atoi.c \
    sbr/m_backup.c \
    sbr/m_convert.c \
    sbr/m_draft.c \
    sbr/m_getfld.c \
    sbr/m_gmprot.c \
    sbr/m_maildir.c \
    sbr/m_mktemp.c \
    sbr/m_name.c \
    sbr/m_popen.c \
    sbr/m_rand.c \
    sbr/makedir.c \
    sbr/md5.c \
    sbr/message_id.c \
    sbr/mf.c \
    sbr/mime_type.c \
    sbr/mts.c \
    sbr/netsec.c \
    sbr/oauth.c \
    sbr/oauth_prof.c \
    sbr/path.c \
    sbr/pidstatus.c \
    sbr/pidwait.c \
    sbr/print_help.c \
    sbr/print_sw.c \
    sbr/print_version.c \
    sbr/push.c \
    sbr/r1bindex.c \
    sbr/read_line.c \
    sbr/read_switch.c \
    sbr/read_switch_multiword.c \
    sbr/read_switch_multiword_via_readline.c \
    sbr/read_yes_or_no_if_tty.c \
    sbr/readconfig.c \
    sbr/refile.c \
    sbr/remdir.c \
    sbr/ruserpass.c \
    sbr/seq_add.c \
    sbr/seq_bits.c \
    sbr/seq_del.c \
    sbr/seq_getnum.c \
    sbr/seq_list.c \
    sbr/seq_nameok.c \
    sbr/seq_print.c \
    sbr/seq_read.c \
    sbr/seq_save.c \
    sbr/seq_setcur.c \
    sbr/seq_setprev.c \
    sbr/seq_setunseen.c \
    sbr/showfile.c \
    sbr/signals.c \
    sbr/smatch.c \
    sbr/snprintb.c \
    sbr/ssequal.c \
    sbr/strindex.c \
    sbr/terminal.c \
    sbr/trimcpy.c \
    sbr/unquote.c \
    sbr/uprf.c \
    sbr/utils.c \
    sbr/vector.c \
    sbr/vfgets.c \
    thirdparty/jsmn/jsmn.c \
    #

sbr_libmh_a_LIBADD = $(LIBOBJS)

##
## Need explicit dependency so that YACC is run before LEX.
##
sbr/icalendar.c: sbr/icalparse.c

##
## Because these files use the definitions in the libmh rule below,
## they need to be rebuilt if the Makefile changes.
##

config/sbr_libmh_a-config.$(OBJEXT) sbr/sbr_libmh_a-mts.$(OBJEXT): Makefile

sbr_libmh_a_CPPFLAGS = ${AM_CPPFLAGS} -I./sbr \
		-DNMHBINDIR='"$(bindir)"' \
		-DNMHLIBEXECDIR='"$(nmhlibexecdir)"' \
		-DNMHETCDIR='"@nmhetcdirinst@$(nmhetcdir)"' \
		-DNMHDOCDIR='"@docdir@"' \
		-DMAILSPOOL='"$(mailspool)"' \
		-DSENDMAILPATH='"$(sendmailpath)"'

mts_libmts_a_SOURCES = mts/smtp/smtp.c

##
## Our rules for generating man pages (both the rule for man.sed and
## the implicit rules for man page generation).
##
manext1 = 1
manext5 = 5
manext7 = 7
manext8 = 8

$(man_MANS): man/man.sed

man/man.sed: Makefile
	@echo 's,%nmhwarning%,THIS FILE HAS BEEN AUTOMATICALLY GENERATED.  DO NOT EDIT.,g' > $@
	@echo 's,%nmhversion%,nmh-$(VERSION),g' >> $@
	@echo 's,%nmhetcdir%,$(nmhetcdir),g' >> $@
	@echo 's,%nmhlibexecdir%,$(nmhlibexecdir),g' >> $@
	@echo 's,%bindir%,$(bindir),g' >> $@
	@echo 's,%docdir%,$(docdir),g' >> $@
	@echo 's,%mandir%,$(mandir),g' >> $@
	@echo 's,%mailspool%,$(mailspool),g' >> $@
	@echo 's,%sendmailpath%,$(sendmailpath),g' >> $@
	@echo 's,%default_locking%,$(default_locking),g' >> $@
	@echo 's,%supported_locks%,$(supported_locks),g' >> $@
	@echo 's,%manext1%,$(manext1),g' >> $@
	@echo 's,%manext5%,$(manext5),g' >> $@
	@echo 's,%manext7%,$(manext7),g' >> $@
	@echo 's,%manext8%,$(manext8),g' >> $@
	@echo '/%mhl_forward%/r $(top_srcdir)/etc/mhl.forward' >> $@
	@echo ' s,%mhl_forward%,,g' >> $@
	@echo '/%mhl_format%/r $(top_srcdir)/etc/mhl.format' >> $@
	@echo ' s,%mhl_format%,,g' >> $@
	@echo '/%mhl_reply%/r $(top_srcdir)/etc/mhl.reply' >> $@
	@echo ' s,%mhl_reply%,,g' >> $@

man/mh-chart.man: $(man_SRCS)
	$(srcdir)/man/mh-chart-gen.sh > $@

.man.$(manext1):
	$(SED) -f man/man.sed $< > $@

.man.$(manext5):
	$(SED) -f man/man.sed $< > $@

.man.$(manext7):
	$(SED) -f man/man.sed $< > $@

.man.$(manext8):
	$(SED) -f man/man.sed $< > $@

man: $(man_MANS)

cleanman:
	rm -f $(man_MANS)

.PHONY: man cleanman

## Don't include commit hashes in ChangeLog.
ChangeLog:
	@[ -d $(srcdir)/.git ]  &&  (cd $(srcdir); git --no-pager log --abbrev-commit | \
           egrep -v '^commit [0-9a-f]+$$' > $@)  ||  true
## Make ChangeLog phony so it will always get regenerated.  But don't
## fail if we don't have a git repository in order to allow
## regeneration of distribution archive or RPM.
.PHONY: ChangeLog

##
## Our RPM build target
##
rpm: dist
	@$(MKDIR_P) $(rpmdir)/SOURCES
	@mv -f $(DIST_ARCHIVES) $(rpmdir)/SOURCES
	@cp -p VERSION $(rpmdir)/SOURCES
	@rpmbuild --define '_topdir $(rpmdir)' \
	  --define '_tmppath %{_topdir}/TMP' \
	  --define "_sysconfdir $(DESTDIR)$(sysconfdir)" \
	  -ba SPECS/nmh.spec
.PHONY: rpm


##
## Use GNU gcov to find the coverage of the test suite.
## These targets are not integrated into the other automake
## targets because they will be used so infrequently, if at all.
##
gcov: gcov-rebuild gcov-run gcov-process gcov-report
gcov-rebuild:
	@echo rebuilding with AM_CFLAGS=--coverage . . .
	@(make clean  &&  make AM_CFLAGS=--coverage) > /dev/null
gcov-run:
	make check AM_CFLAGS=--coverage
gcov-process:
	@for i in `find . -name '*.gcda'`; do \
	   gcov -pro `echo $$i | $(SED) 's%\\(.*\\)/%\\1 %'`; \
	 done
gcov-report:
	@for i in `find . -name '*.gcno'`; do \
	   if test -f `echo $$i | sed 's%\.gcno%.gcda%'`; then :; else \
	     echo untested: $$i; \
	   fi; \
	 done
gcov-mostlyclean:
	@find . -name '*.gcno' -o -name '*.gcda' | xargs rm
gcov-clean: gcov-mostlyclean
	@find . -name '*.gcov' | xargs rm
.PHONY: gcov gcov-rebuild gcov-run gcov-process gcov-report \
    gcov-mostlyclean gcov-clean


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
		gpg --output $(DIST_ARCHIVES).sig --detach-sig $(DIST_ARCHIVES); \
	fi
	@echo "Setting permissions for savannah.gnu.org"
	chmod 664 $(DIST_ARCHIVES)
	if test -f $(DIST_ARCHIVES).sig; then \
		chmod 664 $(DIST_ARCHIVES).sig; \
	fi
	@echo "Uploading to savannah"
	scp -p $(DIST_ARCHIVES)* $(SAVANNAH_USERNAME)@dl.sv.nongnu.org:/releases/nmh
.PHONY: upload
