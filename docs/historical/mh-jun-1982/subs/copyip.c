#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

char **
copyip(cpf, cpt)
char **cpf, **cpt;
{
	register int *ipf, *ipt;

	ipf = (int *) cpf;
	ipt = (int *) cpt;

	while((*ipt = *ipf) && *ipf++ != -1)
		ipt++;
	*ipt = 0;
	return (char **) ipt;
}

