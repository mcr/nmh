#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/*
 * This is where the user mailboxes are kept.  On our 11/70 they were
 * kept in each users $HOME directory.  This is where the VAX login
 * program is looking for them, as well as the old BELL mail.
 */

#include <mailsys.h>

char    *mailboxes =      MAILDROP;
