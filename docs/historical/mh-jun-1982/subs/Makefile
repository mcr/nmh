#
#       Proprietary Rand Corporation, 1981.
#       Further distribution of this software
#       subject to the terms of the Rand
#       license agreement.
#

CFLAGS = -O -I/usr/include -I../support # -m
LIB = subs.a
OFILES =\
	add.o \
	ambigsw.o \
	atooi.o \
	brkstring.o \
	cdate.o \
	cndfree.o \
	concat.o \
	copy.o \
	copyip.o \
	cputc.o \
	done.o \
	fdcompare.o \
	gans.o \
	getans.o \
	getcpy.o \
	help.o \
	locv.o \
	m_convert.o \
	m_delete.o \
	m_edit.o \
	m_file.o \
	m_find.o \
	m_getcur.o \
	m_getdefs.o \
	m_getfld.o \
	m_getfolder.o \
	m_gmprot.o \
	m_gmsg.o \
	m_maildir.o \
	m_name.o \
	m_replace.o \
	m_send.o \
	m_setcur.o \
	m_update.o \
	makedir.o \
	makename.o \
	mu_atoi.o \
	path.o \
	peekc.o \
	pr_array.o \
	printsw.o \
	putenv.o \
	pwd.o \
	r1bindex.o \
	showfile.o \
	smatch.o \
	ssequal.o \
	trimcpy.o \
	type.o \
	uleq.o

CFILES =\
	add.c \
	ambigsw.c \
	atooi.c \
	brkstring.c \
	cdate.c \
	cndfree.c \
	concat.c \
	copy.c \
	copyip.c \
	cputc.c \
	done.c \
	fdcompare.c \
	gans.c \
	getans.c \
	getcpy.c \
	help.c \
	locv.c \
	m_convert.c \
	m_delete.c \
	m_edit.c \
	m_file.c \
	m_find.c \
	m_getcur.c \
	m_getdefs.c \
	m_getfld.c \
	m_getfolder.c \
	m_gmprot.c \
	m_gmsg.c \
	m_maildir.c \
	m_name.c \
	m_replace.c \
	m_send.c \
	m_setcur.c \
	m_update.c \
	makedir.c \
	makename.c \
	mu_atoi.c \
	path.c \
	peekc.c \
	pr_array.c \
	printsw.c \
	putenv.c \
	pwd.c \
	r1bindex.c \
	showfile.c \
	smatch.c \
	ssequal.c \
	trimcpy.c \
	type.c \
	uleq.c

$(LIB): $(OFILES)
	-rm -f $(LIB)
	ar cr $(LIB) `lorder $(OFILES) | tsort`
	ranlib $(LIB)

.c.o:
	$(CC) $(CFLAGS) -c -O $*.c
	-@ld -x -r $@
	@mv a.out $@

lint:
	-lint $(CFILES) subs-lc

distribution:   clean
	-rm -f $(LIB)

clean:
	-rm -f $(OFILES)

depend:
	../misc/depend.sh $(CFILES)

###DEPENDENCIES Follow.  Do not delete this line
add.o:	 /usr/include/stdio.h
add.o:	 ../mh.h
ambigsw.o:	 /usr/include/stdio.h
ambigsw.o:	 ../mh.h
concat.o:	 ../mh.h
cputc.o:	 /usr/include/stdio.h
fdcompare.o:	 ../mh.h
gans.o:	 ../mh.h
gans.o:	 /usr/include/stdio.h
getans.o:	 ../mh.h
getans.o:	 /usr/include/signal.h
getans.o:	 /usr/include/stdio.h
getcpy.o:	 ../mh.h
help.o:	 ../mh.h
locv.o:	 /usr/include/stdio.h
m_convert.o:	 ../mh.h
m_convert.o:	 /usr/include/stdio.h
m_convert.o:	 /usr/include/ctype.h
m_delete.o:	 ../mh.h
m_edit.o:	 ../mh.h
m_edit.o:	 /usr/include/sys/types.h
m_edit.o:	 /usr/include/sys/stat.h
m_edit.o:	 /usr/include/signal.h
m_edit.o:	 /usr/include/stdio.h
#m_edit.o:        /usr/include/strings.h
m_file.o:	 ../mh.h
m_file.o:	 ../folder.h
m_file.o:	 /usr/include/stdio.h
m_file.o:	 /usr/include/sys/types.h
m_file.o:	 /usr/include/sys/stat.h
m_file.o:	 /usr/include/errno.h
m_find.o:	 ../mh.h
m_find.o:	 /usr/include/stdio.h
m_getcur.o:	 /usr/include/stdio.h
m_getcur.o:	 ../mh.h
m_getdefs.o:	 ../mh.h
m_getdefs.o:	 /usr/include/stdio.h
m_getfld.o:	 ../mh.h
m_getfld.o:	 /usr/include/stdio.h
m_getfolder.o:	 ../mh.h
m_getfolder.o:	 /usr/include/stdio.h
m_gmprot.o:	 ../mh.h
m_gmprot.o:	 /usr/include/stdio.h
m_gmsg.o:	 ../mh.h
m_gmsg.o:	 /usr/include/stdio.h
m_maildir.o:	 ../mh.h
m_maildir.o:	 /usr/include/stdio.h
m_name.o:	 ../mh.h
m_replace.o:	 ../mh.h
m_send.o:	 ../mh.h
m_send.o:	 /usr/include/stdio.h
m_setcur.o:	 ../mh.h
m_setcur.o:	 /usr/include/stdio.h
m_update.o:	 ../mh.h
m_update.o:	 /usr/include/stdio.h
m_update.o:	 /usr/include/signal.h
makedir.o:	 ../mh.h
makedir.o:	 /usr/include/stdio.h
makename.o:	 ../mh.h
makename.o:	 /usr/include/stdio.h
mu_atoi.o:	 ../mh.h
path.o:	 ../mh.h
peekc.o:	 /usr/include/stdio.h
peekc.o:	 ../mh.h
printsw.o:	 ../mh.h
putenv.o:	 /usr/include/stdio.h
pwd.o:	 /usr/include/stdio.h
#pwd.o:   /usr/include/strings.h
showfile.o:	 ../mh.h
showfile.o:	 /usr/include/signal.h
showfile.o:	 /usr/include/stdio.h
smatch.o:	 ../mh.h
type.o:	 /usr/include/stdio.h
