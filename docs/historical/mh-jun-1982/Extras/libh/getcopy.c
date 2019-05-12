#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

char copy_space[128];
int copy_size;	/* if this is non-zero, assume represents */
		/* actual size of copy_space */
char *copy_ptr;
char *
getcopy(str)
char *str;
{
	static char *cpend;
	register char *r1, *r2;

	if (cpend == 0) {
		/* first time through */
		cpend = copy_space+ (copy_size? copy_size: sizeof copy_space);
		copy_ptr = copy_space;
	}
	r2 = str;
	for (r1 = copy_ptr; r1 < cpend; ) {
		if ((*r1++ = *r2++) == 0) {
			r2 = copy_ptr;
			copy_ptr = r1;
			return(r2);
		}
	}
	write(2, "getcopy out of space\n", 21);
	exit(-1);
}
