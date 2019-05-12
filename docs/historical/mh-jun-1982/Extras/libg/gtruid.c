#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

/* break up uid into effective and real */
/* more compatible with 16-bit uid's */
/* getuid and setuid are std. bell routines */
geteuid()
{
	return((getuid()>>8)&0377);
}
getruid()
{
	return(getuid()&0377);
}
seteuid(euid)
{
	return(setuid(euid<<8|getuid()&0377));
}
setruid(ruid)
{
	return(setuid(ruid&0377|getuid()&0177600));
}
