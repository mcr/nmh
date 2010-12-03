
/*
 * addrsbr.h -- definitions for the address parsing system
 */

#define	AD_HOST	1		/* getm(): lookup official hostname    */
#define	AD_NHST	0		/* getm(): do not lookup official name */
#define	AD_NAME	AD_NHST		/* AD_HOST is TOO slow                 */

#define	UUCPHOST	(-1)
#define	LOCALHOST	0
#define	NETHOST		1
#define	BADHOST		2

struct mailname {
    struct mailname *m_next;
    char *m_text;
    char *m_pers;
    char *m_mbox;
    char *m_host;
    char *m_path;
    int m_type;
    char m_nohost;
    char m_bcc;
    int m_ingrp;
    char *m_gname;
    char *m_note;
};

#define	adrformat(m) auxformat ((m), 1)

/*
 *  prototypes
 */
void mnfree(struct mailname *);
int ismymbox(struct mailname *);
char *getname(char *);
char *adrsprintf(char *, char *);
char *auxformat(struct mailname *, int);
struct mailname *getm(char *, char *, int, int, char *);
