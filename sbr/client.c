
/*
 * client.c -- connect to a server
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/mts.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HESIOD
# include <hesiod.h>
#endif

#ifdef KPOP
# include <krb.h>
# include <ctype.h>
#endif	/* KPOP */

#define	TRUE         1
#define	FALSE        0

#define	OOPS1      (-2)
#define	OOPS2      (-3)

#define	MAXARGS   1000
#define	MAXNETS      5
#define	MAXHOSTS    25

struct addrent {
    int a_addrtype;		/* assumes AF_INET for inet_netof () */
    union {
	int un_net;
	char un_addr[14];
    } un;
};

#define	a_net  un.un_net
#define	a_addr un.un_addr

static struct addrent *n1, *n2;
static struct addrent nets[MAXNETS];
static struct addrent *h1, *h2;
static struct addrent hosts[MAXHOSTS];

#ifdef KPOP
static CREDENTIALS cred;
static MSG_DAT msg_data;
static KTEXT ticket = (KTEXT) NULL;
static Key_schedule schedule;
static char *kservice;			/* "pop" if using kpop */
char krb_realm[REALM_SZ];
char *PrincipalHostname();
#endif /* KPOP */

#if !defined(h_addr)
# define h_addr h_addr_list[0]
#endif

#define	inaddr_copy(hp,sin) \
    memcpy(&((sin)->sin_addr), (hp)->h_addr, (hp)->h_length)

/*
 * static prototypes
 */
static int rcaux (struct servent *, struct hostent *, int, char *, int);
static int getport (int, int, char *, int);
static int inet (struct hostent *, int);
struct hostent *gethostbystring (char *s);

/* client's own static version of several nmh subroutines */
static char **client_brkstring (char *, char *, char *);
static int client_brkany (char, char *);
static char **client_copyip (char **, char **, int);
static char *client_getcpy (char *);


int
client (char *args, char *protocol, char *service, int rproto,
		char *response, int len_response)
{
    int sd;
    register char **ap;
    char *arguments[MAXARGS];
    register struct hostent *hp;
    register struct servent *sp;
#ifndef	HAVE_GETHOSTBYNAME
    register struct netent *np;
#endif

#ifdef KPOP
    char *cp;

    kservice = service;
    if (cp = strchr (service, '/')) {	/* "pop/kpop" */
	*cp++ = '\0';		/* kservice = "pop" */
	service = cp;		/* service  = "kpop" */
    } else {
	kservice = NULL;	/* not using KERBEROS */
    }
#endif	/* KPOP */
    

    if ((sp = getservbyname (service, protocol)) == NULL) {
#ifdef HESIOD
	if ((sp = hes_getservbyname (service, protocol)) == NULL) {
	    snprintf (response, len_response, "%s/%s: unknown service", protocol, service);
	    return NOTOK;
	}
#else
	snprintf (response, len_response, "%s/%s: unknown service", protocol, service);
	return NOTOK;
#endif
    }

    ap = arguments;
    if (args != NULL && *args != 0) {
	ap = client_copyip (client_brkstring (client_getcpy (args), " ", "\n"),
		ap, MAXARGS);
    } else {
	if (servers != NULL && *servers != 0)
	    ap = client_copyip (client_brkstring (client_getcpy (servers), " ", "\n"),
		ap, MAXARGS);
    }
    if (ap == arguments) {
	*ap++ = client_getcpy ("localhost");
	*ap = NULL;
    }

    n1 = nets;
    n2 = nets + sizeof(nets) / sizeof(nets[0]);

    h1 = hosts;
    h2 = hosts + sizeof(hosts) / sizeof(hosts[0]);

    for (ap = arguments; *ap; ap++) {
	if (**ap == '\01') {
/*
 * the assumption here is that if the system doesn't have a
 * gethostbyname() function, it must not use DNS. So we need to look
 * into the /etc/hosts using gethostent(). There probablly aren't any
 * systems still like this, but you never know. On every system I have
 * access to, this section is ignored.
 */
#ifndef	HAVE_GETHOSTBYNAME
	    if ((np = getnetbyname (*ap + 1))) {
#ifdef HAVE_SETHOSTENT
		sethostent (1);
#endif /* HAVE_SETHOSTENT */
		while ((hp = gethostent()))
		    if (np->n_addrtype == hp->h_addrtype
			    && inet (hp, np->n_net)) {
			switch (sd = rcaux (sp, hp, rproto, response, len_response)) {
			    case NOTOK: 
				continue;
			    case OOPS1: 
				break;
			    case OOPS2: 
				return NOTOK;

			    default: 
				return sd;
			}
			break;
		    }
	    }
#endif /* don't HAVE_GETHOSTBYNAME */
	    continue;
	}

	if ((hp = gethostbystring (*ap))) {
	    switch (sd = rcaux (sp, hp, rproto, response, len_response)) {
		case NOTOK: 
		case OOPS1: 
		    break;
		case OOPS2: 
		    return NOTOK;

		default: 
		    return sd;
	    }
	    continue;
	}
    }

    strncpy (response, "no servers available", len_response);
    return NOTOK;
}


static int
rcaux (struct servent *sp, struct hostent *hp, int rproto,
		char *response, int len_response)
{
    int sd;
    struct in_addr in;
    register struct addrent *ap;
    struct sockaddr_in in_socket;
    register struct sockaddr_in *isock = &in_socket;

#ifdef KPOP
    int rem;
    struct hostent *hp2;
#endif	/* KPOP */

    for (ap = nets; ap < n1; ap++)
	if (ap->a_addrtype == hp->h_addrtype && inet (hp, ap->a_net))
	    return NOTOK;

    for (ap = hosts; ap < h1; ap++)
	if (ap->a_addrtype == hp->h_addrtype
		&& memcmp(ap->a_addr, hp->h_addr, hp->h_length) == 0)
	    return NOTOK;

    if ((sd = getport (rproto, hp->h_addrtype, response, len_response)) == NOTOK)
	return OOPS2;

    memset (isock, 0, sizeof(*isock));
    isock->sin_family = hp->h_addrtype;
    inaddr_copy (hp, isock);
    isock->sin_port = sp->s_port;

    if (connect (sd, (struct sockaddr *) isock, sizeof(*isock)) == NOTOK)
	switch (errno) {
	    case ENETDOWN: 
	    case ENETUNREACH: 
		close (sd);
		if (n1 < n2) {
		    n1->a_addrtype = hp->h_addrtype;
		    memcpy(&in, hp->h_addr, sizeof(in));
		    n1->a_net = inet_netof (in);
		    n1++;
		}
		return OOPS1;

	    case ETIMEDOUT: 
	    case ECONNREFUSED: 
	    default: 
		close (sd);
		if (h1 < h2) {
		    h1->a_addrtype = hp->h_addrtype;
		    memcpy(h1->a_addr, hp->h_addr, hp->h_length);
		    h1++;
		}
		return NOTOK;
	}

#ifdef KPOP
    if (kservice) {	/* "pop" */
	char *instance;

	if (( hp2 = gethostbyaddr( hp->h_addr, hp->h_length, hp->h_addrtype ))
		== NULL ) {
	    return NOTOK;
	}
	if ((instance = strdup (hp2->h_name)) == NULL) {
	    close (sd);
	    strncpy (response, "Out of memory.", len_response);
	    return OOPS2;
	}
	ticket = (KTEXT) malloc (sizeof(KTEXT_ST));
	rem = krb_sendauth (0L, sd, ticket, kservice, instance,
			   (char *) krb_realmofhost (instance),
			   (unsigned long) 0, &msg_data, &cred, schedule,
			   (struct sockaddr_in *) NULL,
			   (struct sockaddr_in *) NULL,
			   "KPOPV0.1");
	free (instance);
	if (rem != KSUCCESS) {
	    close (sd);
	    strncpy (response, "Post office refused connection: ", len_response);
	    strncat (response, krb_err_txt[rem], len_response - strlen(response));
	    return OOPS2;
	}
    }
#endif /* KPOP */

    return sd;
}


static int
getport (int rproto, int addrtype, char *response, int len_response)
{
    int sd, port;
    struct sockaddr_in in_socket, *isock;

    isock = &in_socket;
    if (rproto && addrtype != AF_INET) {
	snprintf (response, len_response, "reserved ports not supported for af=%d", addrtype);
	errno = ENOPROTOOPT;
	return NOTOK;
    }

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == NOTOK) {
	char *s;

	if ((s = strerror (errno)))
	    snprintf (response, len_response, "unable to create socket: %s", s);
	else
	    snprintf (response, len_response, "unable to create socket: unknown error");
	return NOTOK;
    }
#ifdef KPOP
    if (kservice)	/* "pop" */
	return(sd);
#endif	/* KPOP */
    if (!rproto)
	return sd;

    memset(isock, 0, sizeof(*isock));
    isock->sin_family = addrtype;
    for (port = IPPORT_RESERVED - 1;;) {
	isock->sin_port = htons ((unsigned short) port);
	if (bind (sd, (struct sockaddr *) isock, sizeof(*isock)) != NOTOK)
	    return sd;

	switch (errno) {
	    char *s;

	    case EADDRINUSE: 
	    case EADDRNOTAVAIL: 
		if (--port <= IPPORT_RESERVED / 2) {
		    strncpy (response, "ports available", len_response);
		    return NOTOK;
		}
		break;

	    default: 
		if ((s = strerror (errno)))
		    snprintf (response, len_response, "unable to bind socket: %s", s);
		else
		    snprintf (response, len_response, "unable to bind socket: unknown error");
		return NOTOK;
	}
    }
}


static int
inet (struct hostent *hp, int net)
{
    struct in_addr in;

    memcpy(&in, hp->h_addr, sizeof(in));
    return (inet_netof (in) == net);
}


/*
 * taken from ISODE's compat/internet.c
 */

static char *empty = NULL;

#ifdef h_addr
static char *addrs[2] = { NULL };
#endif

struct hostent *
gethostbystring (char *s)
{
    register struct hostent *h;
    static struct hostent hs;
#ifdef DG
    static struct in_addr iaddr;
#else
    static unsigned long iaddr;
#endif

    iaddr = inet_addr (s);
#ifdef DG
    if (iaddr.s_addr == NOTOK && strcmp (s, "255.255.255.255"))
#else
    if (((int) iaddr == NOTOK) && strcmp (s, "255.255.255.255"))
#endif
	return gethostbyname (s);

    h = &hs;
    h->h_name = s;
    h->h_aliases = &empty;
    h->h_addrtype = AF_INET;
    h->h_length = sizeof(iaddr);
#ifdef h_addr
    h->h_addr_list = addrs;
    memset(addrs, 0, sizeof(addrs));
#endif
    h->h_addr = (char *) &iaddr;

    return h;
}


/*
 * static copies of three nmh subroutines
 */

static char *broken[MAXARGS + 1];

static char **
client_brkstring (char *strg, char *brksep, char *brkterm)
{
    register int bi;
    register char c, *sp;

    sp = strg;

    for (bi = 0; bi < MAXARGS; bi++) {
	while (client_brkany (c = *sp, brksep))
	    *sp++ = 0;
	if (!c || client_brkany (c, brkterm)) {
	    *sp = 0;
	    broken[bi] = 0;
	    return broken;
	}

	broken[bi] = sp;
	while ((c = *++sp) && !client_brkany (c, brksep) && !client_brkany (c, brkterm))
	    continue;
    }
    broken[MAXARGS] = 0;

    return broken;
}


/*
 * returns 1 if chr in strg, 0 otherwise
 */
static int
client_brkany (char chr, char *strg)
{
    register char *sp;
 
    if (strg)
	for (sp = strg; *sp; sp++)
	    if (chr == *sp)
		return 1;
    return 0;
}


/*
 * copy a string array and return pointer to end
 */
static char **
client_copyip (char **p, char **q, int len_q)
{
    while (*p && --len_q > 0)
	*q++ = *p++;

    *q = NULL;

    return q;
}


static char *
client_getcpy (char *str)
{
    char *cp;
    size_t len;

    len = strlen(str) + 1;
    if (!(cp = malloc(len)))
	return NULL;

    memcpy (cp, str, len);
    return cp;
}

