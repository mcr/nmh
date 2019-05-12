#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

char *tim_ptr;
int dmsize[];
int timezone;
int no_add;

char *gd_months[] {
	"january",	"february",	"march",	"april",
	"may",		"june",		"july",		"august",
	"september",	"october",	"november",	"december",
	0,
};


long gdate(str)
{
	long tim, now, ghours(), secs;
	int day, month, year, *nowp;
	register int i;
	register char *monp;

	time(&now);
	nowp = localtime(&now);
	tim_ptr = str;
	if(*tim_ptr == '+') {
		tim_ptr++;
		return(now + ghours());
	}
	day = gd_gdec();
	if(*tim_ptr == '-') {
		tim_ptr++;
		if(*tim_ptr >= 'A' && *tim_ptr <= 'z') {
			monp = gd_head();
			if((month = swmtch(monp, &gd_months)) < 0) {
				printf("Month \"%s\" Nonexistant.\n", monp);
				return(0l);
			}
			if(*tim_ptr == '-') {
				tim_ptr++;
				year = gd_gdec();
			} else
				year = nowp[5];
		} else {
			month = nowp[4];
			year = nowp[5];
		}
		if(*tim_ptr == ' ' || *tim_ptr == '@') {
			tim_ptr++;
			secs = ghours();
		} else
			secs = nowp[0]+ nowp[1]*60l + nowp[2]*3600l;
		tim = 0l;
		if(year<1900) year =+ 1900;
		for(i = 1970; i < year; i++)
			tim =+ dysize(i);
		if(dysize(year) == 366 && month >= 3)
			tim++;
		while(month--)
			tim =+ dmsize[month];
		tim =+ day - 1;
		tim =* 24l*60l*60l;
		tim =+ secs;
		tim =+ timezone;
		nowp = localtime(&tim);
		if(nowp[8])
			tim =- 1*60l*60l;
		return(tim);
	}
	tim_ptr = str;
	secs = ghours();
	secs =- nowp[0] + nowp[1]*60l + nowp[2]*3600l;
	if(secs < 0 && no_add == 0)
		secs =+ 24 * 60l * 60l;
	return(now + secs);
}


long ghours()
{
	register char *cp;
	register int h, m;
	int s;

	cp = tim_ptr;
	s = m = 0;
	h = gd_gdec();
	if(tim_ptr - cp == 4 && *tim_ptr != ':') {
		m = h % 100;
		h = h / 100;
	} else {
		if(*tim_ptr == ':') {
			tim_ptr++;
			m = gd_gdec();
			if(*tim_ptr == ':') {
				tim_ptr++;
				s = gd_gdec();
			}
		}
	}
	if(*tim_ptr == 'p') {
		tim_ptr++;
		h =+ 12;
	}
	return(h*3600l + m*60l + s);
}


gd_gdec()
{
	register int c, i;

	i = 0;
	c = *tim_ptr;
	while(c >= '0' && c <= '9') {
		i = i*10 + c - '0';
		c = *++tim_ptr;
	}
	return(i);
}


gd_head()
{
	static char buf[20];
	register char *c2, c;

	c2 = buf;
	while(((c = *tim_ptr) >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z')) {
		if(c <= 'Z')
			c =+ 'a' - 'A';
		if(c2 < &buf[18])
			*c2++ = c;
		tim_ptr++;
	}
	*c2 = 0;
	return(buf);
}
