#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>

char *m_find(str)
	char *str;
{
	register struct node *n;

	m_getdefs();
	for(n = m_defs; n; n = n->n_next)
		if(uleq(n->n_name, str))
			return(n->n_field);
	return(NULL);
}
