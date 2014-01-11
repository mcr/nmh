dnl
dnl Try to see if we have a program that can determine the MIME type
dnl of a particular file
dnl

AC_DEFUN([NMH_MIMETYPEPROC],
[AC_CACHE_CHECK([for a program to provide a MIME type string],
		[nmh_cv_mimetype_proc],
  [nmh_cv_mimetype_proc=
   for mprog in 'file --brief --mime-type' 'file --mime-type'
   do
     AS_IF([$mprog "${srcdir}/configure" > /dev/null 2>&1],
     	   [nmh_cv_mimetype_proc="$mprog"; break])
   done])
AS_IF([test X"$nmh_cv_mimetype_proc" != X],
      [mimetype_proc="\"${nmh_cv_mimetype_proc}\""
       AC_DEFINE_UNQUOTED([MIMETYPEPROC], [$mimetype_proc],
      			  [Program, with arguments, that provides MIME type string.])])])
