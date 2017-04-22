/* smtp.h -- definitions for the nmh SMTP Interface
 */

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
/* TLS flags */
#define S_STARTTLS 0x01
#define S_INITTLS 0x02
#define S_TLSENABLEMASK 0x03
#define S_NOVERIFY 0x04
int sm_init (char *, char *, char *, int, int, int, int, const char *,
             const char *, const char *, int);
int sm_winit (char *, int, int);
int sm_wadr (char *, char *, char *);
int sm_waend (void);
int sm_wtxt (char *, int);
int sm_wtend (void);
int sm_end (int);
char *rp_string (int);

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

#define RP_BOK  '\000'      /* went fine; all done                */
#define RP_BPOK '\100'      /* only the first part got done       */
#define RP_BTNO '\200'      /* temporary failure; try later       */
#define RP_BNO  '\300'      /* not now, nor never; you lose       */

/* FIELD 2:  Basic domain of discourse (3-bits) */

#define RP_CSYN '\000'      /* purely a matter of form            */
#define RP_CGEN '\010'      /* couldn't find anywhere else for it */
#define RP_CCON '\020'      /* data-transfer-related issue        */
#define RP_CUSR '\030'      /* pertaining to the user             */
#define RP_CMAI '\040'      /* specific to mail semantics         */


/*
 * SPECIFIC SUCCESS VALUES
 */

/*
 * Complete Success
 */

/* general-purpose OK */
#define RP_OK   (RP_BOK | RP_CGEN | '\001')

/* message is accepted (w/text) */
#define RP_MOK  (RP_BOK | RP_CMAI | '\000')


/*
 * Partial Success
 */

/* message address is accepted */
#define RP_AOK  (RP_BPOK| RP_CMAI | '\000')


/*
 * SPECIFIC FALURE VALUES
 */

/*
 * Partial Failure
 */

/* foreign host screwed up */
#define RP_BHST (RP_BTNO | RP_CCON | '\001')


/*
 * Complete Failure
 */

/* general-purpose NO */
#define RP_NO   (RP_BNO | RP_CGEN | '\001')

/* bad reply code (PERMANENT ERROR) */
#define RP_RPLY (RP_BNO | RP_CCON | '\001')

/* couldn't deliver */
#define RP_NDEL (RP_BNO | RP_CMAI | '\000')

/* bad parameter */
#define RP_PARM (RP_BNO | RP_CSYN | '\002')

/* command not implemented */
#define RP_UCMD (RP_BNO | RP_CSYN | '\003')

/* unknown user */
#define RP_USER (RP_BNO | RP_CUSR | '\000')


/*
 * MACHINE DEPENDENCY
 *
 * The following treat the value as strictly numeric.
 * It relies on the negative values being numerically
 * negative.
 */

/* is return value negative? */
#define rp_isbad(val)   (((signed char)(val)) < 0)
