#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/*
 * Every NEW message will be created with this protection.  When a
 * message is filed it retains its protection, so this only applies
 * to messages coming in through inc.
 */

char    *msgprot =      "0664";
