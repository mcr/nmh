/* mf.h -- include file for mailbox filters
 */

#include "h/nmh.h"

#ifndef	NOTOK
# define NOTOK (-1)
#endif

#ifndef	OK
# define OK 0
#endif

#ifndef	DONE
# define DONE 1
#endif


struct adrx {
    char *text;
    char *pers;
    char *mbox;
    char *host;
    char *path;
    char *grp;
    int ingrp;
    char *note;
    char *err;
};


/*
 * prototypes
 */
char *legal_person (const char *);
struct adrx *getadrx (const char *, int);
