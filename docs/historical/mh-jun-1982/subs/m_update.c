#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <signal.h>

char    defpath[];

m_update()
{
	FILE *out;
	register struct node *np;
	int (*save)();

	if(def_flags & DEFMOD) {
		save = signal(SIGINT, SIG_IGN);
		if((out = fopen(defpath, "w")) == NULL) {
			fprintf(stderr, "Can't create %s!!\n", defpath);
			done(1);
		}
		for(np = m_defs; np; np = np->n_next)
			fprintf(out, "%s: %s\n", np->n_name, np->n_field);
		VOID fclose(out);
		VOID signal(SIGINT, save);
		def_flags &= ~DEFMOD;
	}
}
