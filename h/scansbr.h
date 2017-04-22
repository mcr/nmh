/*
 * scansbr.h -- definitions for scan()
 */

#define	SCNENC	2		/* message just fine, but encrypted(!!) */
#define	SCNMSG	1		/* message just fine                    */
#define	SCNEOF	0		/* empty message                        */
#define	SCNERR	(-1)		/* error message                        */
#define	SCNNUM	(-2)		/* number out of range                  */
#define	SCNFAT	(-3)		/* fatal error                          */

/*
 * default format for `scan' and `inc'
 */

#ifndef	UK
#define	FORMAT	\
"%4(msg)%<(cur)+%| %>%<{replied}-%?{encrypted}E%| %>\
%02(mon{date})/%02(mday{date})%<{date} %|*%>\
%<(mymbox{from})%<{to}To:%14(decode(friendly{to}))%>%>\
%<(zero)%17(decode(friendly{from}))%>  \
%(decode{subject})%<{body}<<%{body}>>%>\n"
#else
#define	FORMAT	\
"%4(msg)%<(cur)+%| %>%<{replied}-%?{encrypted}E%| %>\
%02(mday{date})/%02(mon{date})%<{date} %|*%>\
%<(mymbox{from})%<{to}To:%14(decode(friendly{to}))%>%>\
%<(zero)%17(decode(friendly{from}))%>  \
%(decode{subject})%<{body}<<%{body}>>%>\n"
#endif

#define	WIDTH  78

/*
 * prototypes
 */
int scan (FILE *, int, int, char *, int, int, int, char *, long, int,
          charstring_t *);
