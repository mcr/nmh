#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/*
 * This is the delivery program called ONLY through send to
 * actually deliver mail to users.  It is fairly small, and
 * must run SUID ROOT, to create new mailboxes.
 */

char    *mh_deliver = "/etc/mh/deliver";
