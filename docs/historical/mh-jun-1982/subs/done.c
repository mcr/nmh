#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* This routine is replaced by some modules if they need to do
 * cleanup.  All exits in the code call done rather than exit.
 */

done(status)
{
	exit(status);
}
