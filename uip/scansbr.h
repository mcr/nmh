/* scansbr.h -- routines to help scan along...
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

#define	SCNMSG	1		/* message just fine                    */
#define	SCNEOF	0		/* empty message                        */
#define	SCNERR	(-1)		/* error message                        */
#define	SCNNUM	(-2)		/* number out of range                  */
#define	SCNFAT	(-3)		/* fatal error                          */

/*
 * default format for `scan' and `inc'
 */

#define	FORMAT	\
"%4(msg)%<(cur)+%| %>%<{replied}-%| %>\
%02(mon{date})/%02(mday{date})%<{date} %|*%>\
%<(mymbox{from})%<{to}To:%14(decode(friendly{to}))%>%>\
%<(zero)%17(decode(friendly{from}))%>  \
%(decode{subject})%<{body}<<%{body}>>%>\n"

#define	WIDTH  78

int scan(FILE *, int, int, char *, int, int, int, char *, long, int, charstring_t *);
void scan_finished(void);
void scan_detect_mbox_style(FILE *);
