#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

#include "../h/stat.h"
#include "../h/dirent.h"

struct dirinfo {
	struct dirent dir;
	char null;
	int dotino;
	int dotdotino;
};

char *rootlist[] {
	"/",
	"/rnd/",
	"/mnt/",
	"/sys/",
	"/bak/",
	"/fsd/",
	0
};
gwd()
{
	return(fullpath(0));
}

fullpath(str)
char *str;
{
	static char wdbuf[128];
	struct dirinfo d;
	register char *wp, *cp, **cpp;
	int fino;
	char *savewp, *markwp;
	int dotdev;
	struct inode statb;

	wp = &wdbuf + 1;
	*--wp = 0;
	markwp = 0;
	if (str) {
		wp = prepend(str, wp);
		if (*wp == '/') return(wp);
		*--wp = 0;
		markwp = wp;
	}
	fino = 0;

	for (;;) {
		wp = scandir(&d, fino, wp);
		if (d.dotino != d.dotdotino) {
			fino = d.dotino;
			chdir("..");
			continue;
		}
		savewp = wp;
		if (d.dotino != ROOTINO) break; /* error */
		stat(".", &statb);
		dotdev = statb.i_dev;
		for (cpp = rootlist; cp = *cpp++; ) {
			if (stat(cp, &statb) < 0) continue;
			if (statb.i_dev == dotdev) {
				wp = prepend(cp, wp);
				break;
			}
		}
		break;
	}
	chdir(savewp);
	if (markwp) *markwp = '/';
	return(wp);
}

scandir(dp, fileino, endwhere)
struct dirinfo *dp;
char *endwhere;
{
	register struct dirinfo *d;
	register char *cp, *wp;
	int fh;

	d = dp;
	d->dotino = 0;
	d->dotdotino = 0;
	if (fileino == 0) wp = endwhere;
	else wp = 0;
	fh = open(".", 0);
	if (fh < 0) return(wp);
	while ((!d->dotino || !d->dotdotino || !wp) &&
	    read(fh, &d->dir, sizeof d->dir) == sizeof d->dir) {
		if (d->dir.d_ino == NULLINO) continue;
		if (!d->dotino && equal(d->dir.d_name, "."))
			d->dotino = d->dir.d_ino;
		else if (!d->dotdotino && equal(d->dir.d_name, ".."))
			d->dotdotino = d->dir.d_ino;
		else if (!wp && d->dir.d_ino == fileino) {
			/* ensure name is null term'd */
			d->null = 0;
			wp = endwhere;
			/* separate with '/' */
			if (*wp) *--wp = '/';
			wp = prepend(d->dir.d_name, wp);
		}
	}
	close(fh);
	return(wp);
}

prepend(prefix, endwhere)
char *prefix, *endwhere;
{
	register char *cp, *wp;

	for (cp = prefix; *cp++; );
	--cp;
	for (wp = endwhere; cp > prefix; *--wp = *--cp);
	return(wp);
}
