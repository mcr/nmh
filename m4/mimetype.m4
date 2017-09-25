dnl Try to see if we have a program that can determine the MIME type
dnl of a particular file
dnl
dnl Assume that if file(1) doesn't support --mime-type, then it's
dnl unusable.  The --mime option to file 4.17 on CentOS 5.9, for
dnl example, prints out a long description after the mime type.  We
dnl don't want that.

AC_DEFUN([NMH_MIMETYPEPROC],
[AC_CACHE_CHECK([for a program to provide a MIME type string],
		[nmh_cv_mimetype_proc],
  [nmh_cv_mimetype_proc=
   for mprog in 'file --brief --dereference --mime-type' \
                'file --dereference --mime-type' \
                'file --brief --mime-type' \
                'file --mime-type'
   do
     AS_IF([$mprog "${srcdir}/configure" > /dev/null 2>&1],
	   [nmh_cv_mimetype_proc="$mprog"; break])
   done])
AS_IF([test X"$nmh_cv_mimetype_proc" != X],
      [mimetype_proc="\"${nmh_cv_mimetype_proc}\""
       AC_DEFINE_UNQUOTED([MIMETYPEPROC], [$mimetype_proc],
		  [Program, with arguments, to provide MIME type.])])])

dnl The OpenBSD 5.4 file (4.24) reports --mime-encoding of text
dnl files as "binary".  Detect that by only accepting "us-ascii".
AC_DEFUN([NMH_MIMEENCODINGPROC],
[AC_CACHE_CHECK([for a program to provide a MIME encoding string],
		[nmh_cv_mimeencoding_proc],
  [nmh_cv_mimeencoding_proc=
   for mprog in 'file --brief --dereference --mime-encoding' \
                'file --dereference --mime-encoding' \
                'file --brief --mime-encoding' \
                'file --mime-encoding'
   do
     AS_IF([$mprog "${srcdir}/DATE" > /dev/null 2>&1],
		AS_CASE([`$mprog "${srcdir}/DATE"`],
			[us-ascii],[nmh_cv_mimeencoding_proc="$mprog"; break]))
   done])
AS_IF([test X"$nmh_cv_mimeencoding_proc" != X],
      [mimeencoding_proc="\"${nmh_cv_mimeencoding_proc}\""
       AC_DEFINE_UNQUOTED([MIMEENCODINGPROC], [$mimeencoding_proc],
		  [Program, with arguments, to provide MIME encoding.])])])
