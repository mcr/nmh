SHELL = /bin/sh
VERSION = 3.0
LIBDEFS = -a 0x63f00000 -g 4096 -j 0x1000

LIBCONFIG = ../config/config.j ../config/version.j
LIBMH = ../sbr/add.j ../sbr/addrsbr.j ../sbr/adios.j ../sbr/admonish.j \
	../sbr/advertise.j ../sbr/advise.j ../sbr/ambigsw.j ../sbr/atooi.j \
	../sbr/brkstring.j ../sbr/closefds.j ../sbr/concat.j ../sbr/copy.j \
	../sbr/copyip.j ../sbr/cpydata.j ../sbr/cpydgst.j ../sbr/discard.j \
	../sbr/done.j ../sbr/fdcompare.j ../sbr/fmtcompile.j \
	../sbr/formataddr.j ../sbr/formatdef.j ../sbr/formatsbr.j \
	../sbr/gans.j ../sbr/getans.j ../sbr/getanswer.j ../sbr/getcpy.j \
	../sbr/help.j ../sbr/m_atoi.j ../sbr/m_backup.j ../sbr/m_convert.j \
	../sbr/m_delete.j ../sbr/m_draft.j ../sbr/m_find.j ../sbr/m_fmsg.j \
	../sbr/m_foil.j ../sbr/m_getdefs.j ../sbr/m_getfld.j \
	../sbr/m_getfolder.j ../sbr/m_gmprot.j ../sbr/m_gmsg.j \
	../sbr/m_maildir.j ../sbr/m_msgdef.j ../sbr/m_name.j \
	../sbr/m_readefs.j ../sbr/m_remsg.j ../sbr/m_replace.j \
	../sbr/m_scratch.j ../sbr/m_seq.j ../sbr/m_seqbits.j \
	../sbr/m_seqflag.j ../sbr/m_seqnew.j ../sbr/m_setcur.j \
	../sbr/m_setseq.j ../sbr/m_setvis.j ../sbr/m_sync.j ../sbr/m_tmpfil.j \
	../sbr/m_update.j ../sbr/makedir.j ../sbr/path.j ../sbr/peekc.j \
	../sbr/pidstatus.j ../sbr/pidwait.j ../sbr/printsw.j ../sbr/push.j \
	../sbr/putenv.j ../sbr/pwd.j ../sbr/r1bindex.j ../sbr/refile.j \
	../sbr/remdir.j ../sbr/showfile.j ../sbr/smatch.j ../sbr/sprintb.j \
	../sbr/ssequal.j ../sbr/strindex.j ../sbr/trimcpy.j ../sbr/uleq.j \
	../sbr/uprf.j ../sbr/vfgets.j
LIBMTS = ../mts/sendmail/hosts.j ../mts/sendmail/smail.j
LIBZOT = ../zotnet/bboards/getbbent.j ../zotnet/mf/mf.j \
    ../zotnet/tws/dtime.j ../zotnet/tws/dtimep.j ../zotnet/tws/lexstring.j \
    ../zotnet/mts/client.j ../zotnet/mts/lock.j ../zotnet/mts/mts.j

all: libmh.so.$(VERSION) libconfig.sa libmh.sa libmts.sa libzot.sa
	/usr/dll/bin/verify-shlib -l libmh.so.$(VERSION) \
	    libconfig.sa libmh.sa libmts.sa libzot.sa

libmh.so.$(VERSION): jump.params $(LIBCONFIG) $(LIBMH) $(LIBMTS) $(LIBZOT)
	ar rc libconfig.a $(LIBCONFIG)
	ar rc libmh.a $(LIBMH)
	ar rc libmts.a $(LIBMTS)
	ar rc libzot.a $(LIBZOT)
	/usr/dll/bin/mkimage -l /lib/libmh -v $(VERSION) $(LIBDEFS) -- \
	    libconfig.a libmh.a libmts.a libzot.a `cc --print` -lc
	rm -f libconfig.a libmh.a libmts.a libzot.a

libconfig.sa libmh.sa libmts.sa libzot.sa: libmh.so.$(VERSION)
	/usr/dll/bin/mkstubs -l /lib/libmh -v $(VERSION) $(LIBDEFS) -- \
	    libconfig libmh libmts libzot

inst-all:
	cp libmh.so.$(VERSION) /lib
	/etc/ldconfig /lib

clean:
	-rm *.[csoa] *.so.*.* *.sa verify.out
