#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* Conditional free -- perform a free call if the address passed
 * is in free storage;  else NOP
 */


cndfree(addr)
char *addr;
{
	extern char end;

	if(addr >= &end) free(addr);
}
