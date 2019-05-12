#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif

struct  mailname {
	struct mailname *m_next;
	char            *m_text,
			*m_headali,    /***/
			*m_mbox,
			*m_at,
			*m_host,
			*m_hs, *m_he;
	long            m_hnum;
	char            m_nohost;
}  *getm();

char *adrformat(), *getname(), *stdhost();

struct  hosts {
	struct hosts *nh_next;
	char *nh_name;
	long nh_num;
} hosts;

#ifdef ARPANET
long gethnum();
#else
#define HOSTNAME "(local)"
#define HOSTNUM  0
#endif
/* If on the ARPANET, whoami.h should have these defined. */
/* If not on the ARPANET, delete HOSTNAME, HOSTNUM from whoami.h */
