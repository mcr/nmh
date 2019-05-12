#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif


extern errno;

main()
{
	execl("foo", "foo", 0);
	printf("errno=%d\n", errno);
}
