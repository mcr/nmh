
/*
 * rcvmail.h -- rcvmail hook definitions
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <mts/smtp/smtp.h>


#define RCV_MOK	0
#define RCV_MBX	1


#ifdef NRTC			/* sigh */
# undef RCV_MOK
# undef RCV_MBX
# define RCV_MOK	RP_MOK
# define RCV_MBX	RP_MECH
#endif /* NRTC */
