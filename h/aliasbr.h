/* aliasbr.h -- definitions for the aliasing system
 *
 */

extern char *AliasFile;		/* mh-alias(5)             */

struct aka {
    char *ak_name;		/* name to match against             */
    struct adr *ak_addr;	/* list of addresses that it maps to */
    struct aka *ak_next;	/* next aka in list                  */
    bool ak_visible;		/* should be visible in headers      */
};

struct adr {
    char *ad_text;		/* text of this address in list        */
    struct adr *ad_next;	/* next adr in list                    */
    char ad_local;		/* text is local (check for expansion) */
};

/*
 * prototypes
 */
int alias (char *);
int akvisible (void) PURE;
char *akresult (struct aka *);
char *akvalue (char *);
char *akerror (int);

/* codes returned by alias() */

#define	AK_OK		0	/* file parsed OK	 */
#define	AK_NOFILE	1	/* couldn't read file	 */
#define	AK_ERROR	2	/* error parsing file	 */
#define	AK_LIMIT	3	/* memory limit exceeded */
