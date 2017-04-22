/* dropsbr.h -- definitions for maildrop-style files
 */

/*
 * A file which is formatted like a maildrop may have a corresponding map
 * file which is an index to the bounds of each message.  The first record
 * of such an map is special, it contains:
 *
 *      d_id    = number of messages in file
 *      d_size	= version number of map
 *      d_start = last message read
 *      d_stop  = size of file
 *
 *  Each record after that contains:
 *
 *      d_id	= BBoard-ID: of message, or similar info
 *      d_size	= size of message in ARPA Internet octets (\n == 2 octets)
 *      d_start	= starting position of message in file
 *      d_stop	= stopping position of message in file
 *
 * Note that d_start/d_stop do NOT include the message delimiters, so
 * programs using the map can simply fseek to d_start and keep reading
 * until the position is at d_stop.
 */

/*
 * various formats for maildrop files
 */
#define OTHER_FORMAT 0
#define MBOX_FORMAT  1
#define MMDF_FORMAT  2

#define	DRVRSN 3

struct drop {
    int   d_id;
    int	  d_size;
    off_t d_start;
    off_t d_stop;
};

/*
 * prototypes
 */
int mbx_open (char *, int, uid_t, gid_t, mode_t);
int mbx_read (FILE *, long, struct drop **);
int mbx_write(char *, int, FILE *, int, long, long, off_t, int, int);
int mbx_copy (char *, int, int, int, int, char *, int);
int mbx_size (int, off_t, off_t);
int mbx_close (char *, int);
char *map_name (char *);
int map_read (char *, long, struct drop **, int);
int map_write (char *, int, int, long, off_t, off_t, long, int, int);
int map_chk (char *, int, struct drop *, long, int);
