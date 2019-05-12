#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../mh.h"
#include <stdio.h>
#include <sys/types.h>
#include <stat.h>
#include <strings.h>
#include <signal.h>

extern struct swit anoyes[];    /* Std no/yes gans array */

char   *vec[20];
int     vecp = 1;

struct swit switches[] = {
	"debug",         -5,      /* 0 */
	"draft",          0,      /* 1 */
	"format",         0,      /* 2 */
	"noformat",       0,      /* 3 */
	"msgid",          0,      /* 4 */
	"nomsgid",        0,      /* 5 */
	"verbose",        0,      /* 6 */
	"noverbose",      0,      /* 7 */
	"help",           4,      /* 8 */
	0,                0
};

int     debug;


/*ARGSUSED*/
main(argc, argv)
char *argv[];
{
	register char *drft, *cp;
	register int i;
	int status, pid;
	struct stat stbuf;
	char **ap;
	char *arguments[50], **argp;

	invo_name = argv[0];
#ifdef NEWS
	m_news();
#endif
	drft = 0;
	cp = r1bindex(argv[0], '/');
	if((cp = m_find(cp)) != NULL) {
		ap = brkstring(cp = getcpy(cp), " ", "\n");
		ap = copyip(ap, arguments);
	} else
		ap = arguments;
	copyip(argv+1, ap);
	argp = arguments;
	while(cp = *argp++) {
		if(*cp == '-')
			switch(smatch(++cp, switches)) {
			case -2:ambigsw(cp, switches);       /* ambiguous */
				goto leave;
							     /* unknown */
			case -1:fprintf(stderr, "send: -%s unknown\n", cp);
				goto leave;
							     /* -draft */
			case 1: vec[vecp++] = drft = m_maildir(draft);
				continue;
			case 0: debug++;
			case 2: case 3: case 4:
			case 5: case 6: case 7:
				vec[vecp++] = --cp;
				continue;
			case 8: help("send [file]   [switches]",
				     switches);
				goto leave;
			}
		if(drft) {
			fprintf(stderr, "Send: Only one message at a time.\n");
			done(1);
		}
		vec[vecp++] = drft = cp;
	}
	if(!drft) {
		drft = m_maildir(draft);
		if(stat(drft, &stbuf) == -1) {
			fprintf(stderr, "Draft file: %s doesn't exist.\n", drft);
			done(1);
		}
		cp = concat("Use \"", drft, "\"? ", 0);
		if(!gans(cp, anoyes))
			done(0);
		vec[vecp++] = drft;
	} else {
		if(stat(drft, &stbuf) == -1) {
			fprintf(stderr, "Draft file: %s doesn't exist.\n", drft);
			done(1);
		}
	}
	m_update();
	vec[vecp] = 0;
	vec[0] = r1bindex(mh_deliver, '/');

	while((pid = fork()) == -1) {
		fprintf(stderr, "Waiting for a fork\n");
		sleep(2);
	}
	if(pid == 0) {
		execv(mh_deliver, vec);
		perror(mh_deliver);
		done(1);
	}
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	while((i = wait(&status)) != -1 && i != pid) ;
	if(status == 0 && !debug)
		backup(drft);

leave:  ;
/***    m_update();     ***/
}


backup(file)
char *file;
{
	char buf[128];
	register char *cp;

	buf[0] = 0;
	if(cp = rindex(file, '/'))
		sprintf(buf, "%.*s", (++cp)-file, file);
	else
		cp = file;
	strcat(buf, ",");
	strcat(buf, cp);
	unlink(buf);
	if(link(file, buf) < 0 || unlink(file) < 0) {
		fprintf(stderr, "Send: Backup rename failure ");
		perror(buf);
		done(1);
	}
}
