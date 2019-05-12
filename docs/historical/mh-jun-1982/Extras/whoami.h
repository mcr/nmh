/* USE THIS FILE AS AN EXAMPLE ONLY */
/* You must fill in your own stuff in your whoami.h, which you already have */

#define sysname "youruucpname"

/* Omit these if not on the ARPANET */
#define HOSTNAME "ARPA-NAME"    /* Arpa STANDARD host name for us */
#define HOSTNUM  nnn            /* Short style host number */
#define HOSTHOST nnn            /* For long style host/imp numbers */
#define HOSTIMP  nnn            /*  ditto */

/* HOSTNUM = (HOSTHOST<<6)|HOSTIMP  which only works if host is 0-3 */
/*   since result must fit in a byte.   */
