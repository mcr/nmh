#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#define ECHO 010
/* set, clear echo on file handle # 2 */

int tt_mode[3];
noecho()
{
	gtty(2,tt_mode);
	tt_mode[2] =& ~ECHO;
	return(stty(2,tt_mode));
}

doecho()
{
	gtty(2,tt_mode);
	tt_mode[2] =| ECHO;
	return(stty(2,tt_mo