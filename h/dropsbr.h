/* dropsbr.h -- definitions for maildrop-style files
 */

/*
 * various formats for maildrop files
 */
#define OTHER_FORMAT 0
#define MBOX_FORMAT  1
#define MMDF_FORMAT  2

/*
 * prototypes
 */
int mbx_open (char *, int, uid_t, gid_t, mode_t);
int mbx_copy (char *, int, int, int, char *);
int mbx_close (char *, int);
