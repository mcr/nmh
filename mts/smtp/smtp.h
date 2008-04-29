
/*
 * smtp.h -- definitions for the nmh SMTP Interface
 *
 * $Id$
 */

/* various modes for SMTP */
#define	S_MAIL	0
#define	S_SEND	1
#define	S_SOML	2
#define	S_SAML	3

/* length is the length of the string in text[], which is also NUL
 * terminated, so s.text[s.length] should always be 0.
 */
struct smtp {
    int code;
    int length;
    char text[BUFSIZ];
};

/*
 * prototypes
 */
/* int client (); */
int sm_init (char *, char *, int, int, int, int, int, int, char *, char *);
int sm_winit (int, char *);
int sm_wadr (char *, char *, char *);
int sm_waend (void);
int sm_wtxt (char *, int);
int sm_wtend (void);
int sm_end (int);
char *rp_string (int);

#ifdef MPOP
int sm_bulk (char *);
#endif


/* The remainder of this file is derived from "mmdf.h" */

/*
 *     Copyright (C) 1979,1980,1981,1982,1983  University of Delaware
 *     Used by permission, May, 1984.
 */

/*
 *     MULTI-CHANNEL MEMO DISTRIBUTION FACILITY  (MMDF)
 *     
 *
 *     Copyright (C) 1979,1980,1981,1982,1983  University of Delaware
 *     
 *     Department of Electrical Engineering
 *     University of Delaware
 *     Newark, Delaware  19711
 *
 *     Phone:  (302) 738-1163
 *     
 *     
 *     This program module was developed as part of the University
 *     of Delaware's Multi-Channel Memo Distribution Facility (MMDF).
 *     
 *     Acquisition, use, and distribution of this module and its listings
 *     are subject restricted to the terms of a license agreement.
 *     Documents describing systems using this module must cite its source.
 *
 *     The above statements must be retained with all copies of this
 *     program and may not be removed without the consent of the
 *     University of Delaware.
 *     
 */

/*                      Reply Codes for MMDF
 *
 *  Based on: "Revised FTP Reply Codes", by Jon Postel & Nancy Neigus Arpanet
 *      RFC 640 / NIC 30843, in the "Arpanet Protocol Handbook", E.  Feinler
 *      and J. Postel (eds.), NIC 7104, Network Information Center, SRI
 *      International:  Menlo Park, CA.  (NTIS AD-A0038901)
 *
 *  Actual values are different, but scheme is same.  Codes must fit into
 *  8-bits (to pass on exit() calls); fields are packed 2-3-3 and interpreted
 *  as octal numbers.
 *
 *  Basic format:
 *
 *      0yz: positive completion; entire action done
 *      1yz: positive intermediate; only part done
 *      2yz: Transient negative completion; may work later
 *      3yz: Permanent negative completion; you lose forever
 *
 *      x0z: syntax
 *      x1z: general; doesn't fit any other category
 *      x2z: connections; truly transfer-related
 *      x3z: user/authentication/account
 *      x4x: mail
 *      x5z: file system
 *
 *      3-bit z field is unique to the reply.  In the following,
 *      the RP_xVAL defines are available for masking to obtain a field.
 */

/*
 * FIELD DEFINITIONS & BASIC VALUES
 */

/* FIELD 1:  Basic degree of success (2-bits) */

#define RP_BTYP '\200'      /* good vs. bad; on => bad            */
#define RP_BVAL '\300'      /* basic degree of success            */

#define RP_BOK  '\000'      /* went fine; all done                */
#define RP_BPOK '\100'      /* only the first part got done       */
#define RP_BTNO '\200'      /* temporary failure; try later       */
#define RP_BNO  '\300'      /* not now, nor never; you lose       */

/* FIELD 2:  Basic domain of discourse (3-bits) */

#define RP_CVAL '\070'      /* basic category (domain) of reply   */

#define RP_CSYN '\000'      /* purely a matter of form            */
#define RP_CGEN '\010'      /* couldn't find anywhere else for it */
#define RP_CCON '\020'      /* data-transfer-related issue        */
#define RP_CUSR '\030'      /* pertaining to the user             */
#define RP_CMAI '\040'      /* specific to mail semantics         */
#define RP_CFIL '\050'      /* file system                        */
#define RP_CLIO '\060'      /* local i/o system                   */

/* FIELD 3:  Specific value for this reply (3-bits) */

#define RP_SVAL '\007'      /* specific value of reply            */


/*
 * SPECIFIC SUCCESS VALUES
 */

/*
 * Complete Success
 */

/* done (e.g., w/transaction) */
#define RP_DONE (RP_BOK | RP_CGEN | '\000')

/* general-purpose OK */
#define RP_OK   (RP_BOK | RP_CGEN | '\001')

/* message is accepted (w/text) */
#define RP_MOK  (RP_BOK | RP_CMAI | '\000')


/*
 * Partial Success
 */

/* you are the requestor */
#define RP_MAST (RP_BPOK| RP_CGEN | '\000')

/* you are the requestee */
#define RP_SLAV (RP_BPOK| RP_CGEN | '\001')

/* message address is accepted */
#define RP_AOK  (RP_BPOK| RP_CMAI | '\000')


/*
 * SPECIFIC FALURE VALUES
 */

/*
 * Partial Failure
 */

/* not now; maybe later */
#define RP_AGN  (RP_BTNO | RP_CGEN | '\000')

/* timeout */
#define RP_TIME (RP_BTNO | RP_CGEN | '\001')

/* no-op; nothing done, this time */
#define RP_NOOP (RP_BTNO | RP_CGEN | '\002')

/* encountered an end of file */
#define RP_EOF  (RP_BTNO | RP_CGEN | '\003')

/* channel went bad */
#define RP_NET  (RP_BTNO | RP_CCON | '\000')

/* foreign host screwed up */
#define RP_BHST (RP_BTNO | RP_CCON | '\001')

/* host went away */
#define RP_DHST (RP_BTNO | RP_CCON | '\002')

/* general net i/o problem */
#define RP_NIO  (RP_BTNO | RP_CCON | '\004')

/* error reading/writing file */
#define RP_FIO  (RP_BTNO | RP_CFIL | '\000')

/* unable to create file */
#define RP_FCRT (RP_BTNO | RP_CFIL | '\001')

/* unable to open file */
#define RP_FOPN (RP_BTNO | RP_CFIL | '\002')

/* general local i/o problem */
#define RP_LIO  (RP_BTNO | RP_CLIO | '\000')

/* resource currently locked */
#define RP_LOCK (RP_BTNO | RP_CLIO | '\001')


/*
 * Complete Failure
 */

/* bad mechanism/path; try alternate? */
#define RP_MECH (RP_BNO | RP_CGEN | '\000')

/* general-purpose NO */
#define RP_NO   (RP_BNO | RP_CGEN | '\001')

/* general prototocol error */
#define RP_PROT (RP_BNO | RP_CCON | '\000')

/* bad reply code (PERMANENT ERROR) */
#define RP_RPLY (RP_BNO | RP_CCON | '\001')

/* couldn't deliver */
#define RP_NDEL (RP_BNO | RP_CMAI | '\000')

/* couldn't parse the request */
#define RP_HUH  (RP_BNO | RP_CSYN | '\000')

/* no such command defined */
#define RP_NCMD (RP_BNO | RP_CSYN | '\001')

/* bad parameter */
#define RP_PARM (RP_BNO | RP_CSYN | '\002')

/* command not implemented */
#define RP_UCMD (RP_BNO | RP_CSYN | '\003')

/* unknown user */
#define RP_USER (RP_BNO | RP_CUSR | '\000')


/*
 * Macros to access reply info
 */

/* get the entire return value */
#define rp_gval(val)    ((signed char) (val))


/*
 * The next three give the field's bits, within the whole value
 */

/* get the basic part of return value */
#define rp_gbval(val)   (rp_gval (val) & RP_BVAL)

/* get the domain part of value */
#define rp_gcval(val)   (rp_gval (val) & RP_CVAL)

/* get the specific part of value */
#define rp_gsval(val)   (rp_gval (val) & RP_SVAL)


/*
 * The next three give the numeric value withing the field
 */

/* get the basic part right-shifted */
#define rp_gbbit(val)   ((rp_gval (val) >> 6) & 03)

/* get the domain part right-shifted */
#define rp_gcbit(val)   ((rp_gval (val) >> 3) & 07)

/* get the specific part right-shifted */
#define rp_gsbit(val)   (rp_gval (val) & 07)


/*
 * MACHINE DEPENDENCY
 *
 * The following treat the value as strictly numeric.
 * It relies on the negative values being numerically
 * negative.
 */

/* is return value positive? */
#define rp_isgood(val)  (rp_gval (val) >= 0)

/* is return value negative? */
#define rp_isbad(val)   (rp_gval (val) < 0)

