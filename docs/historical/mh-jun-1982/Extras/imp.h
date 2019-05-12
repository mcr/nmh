#ifdef COMMENT
	Proprietary Rand Corporation, 1981.
	Further distribution of this software
	subject to the terms of the Rand
	license agreement.
#endif


#ifdef  OLDNCP
struct netopen  /* Format of open request */
{
	long no_lskt;   /* Local socket */
	long no_fskt;   /* Foreign socket */
	short no_flags; /* Flag bits */
	short no_rel;   /* Local socket relative to this file */
	short no_imp;   /* Imp number */
	char no_host;   /* Host within Imp */
	char no_net;    /* Network number (!) */
	char no_bsize;  /* Byte size */
};
#else
struct netopen  /* Format of open request */
{
	long no_lskt;   /* Local socket */
	long no_fskt;   /* Foreign socket */
	short no_flags; /* Flag bits */
	short no_rel;   /* Local socket relative to this file */
	short no_imp;   /* Imp number */
	char no_host;   /* Host within Imp */
	char no_net;    /* Network number (!) */
	char no_bsize;  /* Byte size */
};
#endif

#define NO_ABS 01       /* Absolute local socket number for a connect */
#define NO_REL 02       /* Relative to another file */

#define NIMPS   20      /* number of imp devices */
#define MAX_IMPS  10    /* number of imp devices, starting from imp1 */

#define HOSTTBL "/etc/nethosts"

/* Op codes */
#define NET_CNCT 0
#define NET_RD   1
#define NET_WRT  2
#define NET_CLS  3
#define NET_LSN  4
#define NET_SICP 5
#define NET_UICP 6
#define NET_RST  7
#define NET_STRT 8
#define NET_LOG  9
#define NET_RSTAT 10
#define NET_WSTAT 11
#define NET_SRVR  12
#define NET_DUMP  13
#define NET_FLOW 255

/* ioctl commands for imp device*/
#define IMPCONCT        (('I'<<8)|NET_CNCT)    /* Connect */
#define IMPLISTN        (('I'<<8)|NET_LSN )    /* Listen */
#define IMPSICP         (('I'<<8)|NET_SICP)    /* Server ICP */
#define IMPUICP         (('I'<<8)|NET_UICP)    /* User ICP */
#define IMPRST          (('I'<<8)|NET_RST )    /* Reset */
#define IMPSTRT         (('I'<<8)|NET_STRT)    /* Start NCP */
#define IMPLOG          (('I'<<8)|NET_LOG )    /* Set log flags */
#define IMPRSTAT        (('I'<<8)|NET_RSTAT)   /* Read socket status */
#define IMPWSTAT        (('I'<<8)|NET_WSTAT)   /* Write socket status */
#define IMPRDST         (('I'<<8)|100     )  /* Get read status */
#define IMPWRTST        (('I'<<8)|101     )  /* Get write status */

/* Structure for IMPSTAT ioctl */
struct sockstat {
	long sck_lskt;
	long sck_fskt;
	short sck_imp;
	char sck_host;
	char sck_net;
};

long hostnum();
char *hos