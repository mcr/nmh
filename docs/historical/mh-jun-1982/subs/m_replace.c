#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
char *malloc();

m_replace(key,value)
char *key, *value;

{
	register struct node *np;

	m_getdefs();
	for(np = m_defs; ; np = np->n_next) {
		if(uleq(np->n_name, key)) {
			if(strcmp(value, np->n_field) != 0) {
				cndfree(np->n_field);
				np->n_field = value;
				def_flags |= DEFMOD;
			}
			return;
		}
		if(!np->n_next)
			break;
	}
	np->n_next = (struct node *) malloc(sizeof *np);
	np = np->n_next;
	np->n_name = getcpy(key);
	np->n_next = 0;
	np->n_field = value;
	def_flags |= DEFMOD;
}
