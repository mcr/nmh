/* m_getfld.c -- read/parse a message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mts.h>
#include <h/utils.h>

/*
   Purpose
   =======
   Reads an Internet message (RFC 5322), or one or more messages
   stored in a maildrop in mbox (RFC 4155) or MMDF format, from a file
   stream.  Each call to m_getfld() reads one header field, or a
   portion of the body, in sequence.

   Inputs
   ======
   gstate:  opaque parse state
   bufsz:  maximum number of characters to load into buf
   iob:  input file stream

   Outputs
   =======
   name:  header field name (array of size NAMESZ=999)
   buf:  either a header field body or message body
   bufsz:  number of characters loaded into buf
   (return value):  message parse state on return from function

   Functions
   =========
   void m_getfld_state_destroy (m_getfld_state_t *gstate): destroys
   the parse state pointed to by the gstate argument.

   m_getfld_state_reset (m_getfld_state_t *gstate): resets the parse
   state to FLD.

   void m_unknown(FILE *iob):  Determines the message delimiter string
   for the maildrop.  Called by inc and scan when reading from a
   maildrop file.

   State variables
   ===============
   m_getfld() retains state internally between calls in the
   m_getfld_state_t variable.  These are used for detecting the end of
   each message when reading maildrops:

     char **pat_map
     char *fdelim
     char *delimend
     int fdelimlen
     char *edelim
     int edelimlen
     char *msg_delim
     int msg_style

   Usage
   =====
   m_getfld_state_t gstate = 0;
      ...
   int state = m_getfld (&gstate, ...);
      ...
   m_getfld_state_destroy (&gstate);

   The state is retained internally by gstate.  To reset its state to FLD:
   m_getfld_state_reset (&gstate);
*/

/* The following described the old implementation.  The high-level
   structure hasn't changed, but some of the details have.  I'm
   leaving this as-is, though, for posterity.
 */

/* This module has a long and checkered history.  First, it didn't burst
   maildrops correctly because it considered two CTRL-A:s in a row to be
   an inter-message delimiter.  It really is four CTRL-A:s followed by a
   newline.  Unfortunately, MMDF will convert this delimiter *inside* a
   message to a CTRL-B followed by three CTRL-A:s and a newline.  This
   caused the old version of m_getfld() to declare eom prematurely.  The
   fix was a lot slower than

		c == '\001' && peekc (iob) == '\001'

   but it worked, and to increase generality, MBOX style maildrops could
   be parsed as well.  Unfortunately the speed issue finally caught up with
   us since this routine is at the very heart of MH.

   To speed things up considerably, the routine Eom() was made an auxiliary
   function called by the macro eom().  Unless we are bursting a maildrop,
   the eom() macro returns FALSE saying we aren't at the end of the
   message.

   The next thing to do is to read the mts.conf file and initialize
   delimiter[] and delimlen accordingly...

   After mhl was made a built-in in msh, m_getfld() worked just fine
   (using m_unknown() at startup).  Until one day: a message which was
   the result of a bursting was shown. Then, since the burst boundaries
   aren't CTRL-A:s, m_getfld() would blinding plunge on past the boundary.
   Very sad.  The solution: introduce m_eomsbr().  This hook gets called
   after the end of each line (since testing for eom involves an fseek()).
   This worked fine, until one day: a message with no body portion arrived.
   Then the

		   while (eom (c = Getc (iob), iob))
			continue;

   loop caused m_getfld() to return FMTERR.  So, that logic was changed to
   check for (*eom_action) and act accordingly.

   This worked fine, until one day: someone didn't use four CTRL:A's as
   their delimiters.  So, the bullet got bit and we read mts.h and
   continue to struggle on.  It's not that bad though, since the only time
   the code gets executed is when inc (or msh) calls it, and both of these
   have already called mts_init().

   ------------------------
   (Written by Van Jacobson for the mh6 m_getfld, January, 1986):

   This routine was accounting for 60% of the cpu time used by most mh
   programs.  I spent a bit of time tuning and it now accounts for <10%
   of the time used.  Like any heavily tuned routine, it's a bit
   complex and you want to be sure you understand everything that it's
   doing before you start hacking on it.  Let me try to emphasize
   that:  every line in this atrocity depends on every other line,
   sometimes in subtle ways.  You should understand it all, in detail,
   before trying to change any part.  If you do change it, test the
   result thoroughly (I use a hand-constructed test file that exercises
   all the ways a header name, header body, header continuation,
   header-body separator, body line and body eom can align themselves
   with respect to a buffer boundary).  "Minor" bugs in this routine
   result in garbaged or lost mail.

   If you hack on this and slow it down, I, my children and my
   children's children will curse you.

   This routine gets used on three different types of files: normal,
   single msg files, "packed" unix or mmdf mailboxs (when used by inc)
   and packed, directoried bulletin board files (when used by msh).
   The biggest impact of different file types is in "eom" testing.  The
   code has been carefully organized to test for eom at appropriate
   times and at no other times (since the check is quite expensive).
   I have tried to arrange things so that the eom check need only be
   done on entry to this routine.  Since an eom can only occur after a
   newline, this is easy to manage for header fields.  For the msg
   body, we try to efficiently search the input buffer to see if
   contains the eom delimiter.  If it does, we take up to the
   delimiter, otherwise we take everything in the buffer.  (The change
   to the body eom/copy processing produced the most noticeable
   performance difference, particularly for "inc" and "show".)

   There are three qualitatively different things this routine busts
   out of a message: field names, field text and msg bodies.  Field
   names are typically short (~8 char) and the loop that extracts them
   might terminate on a colon, newline or max width.  I considered
   using a Vax "scanc" to locate the end of the field followed by a
   "memmove" but the routine call overhead on a Vax is too large for this
   to work on short names.  If Berkeley ever makes "inline" part of the
   C optimiser (so things like "scanc" turn into inline instructions) a
   change here would be worthwhile.

   Field text is typically 60 - 100 characters so there's (barely)
   a win in doing a routine call to something that does a "locc"
   followed by a "bmove".  About 30% of the fields have continuations
   (usually the 822 "received:" lines) and each continuation generates
   another routine call.  "Inline" would be a big win here, as well.

   Messages, as of this writing, seem to come in two flavors: small
   (~1K) and long (>2K).  Most messages have 400 - 600 bytes of headers
   so message bodies average at least a few hundred characters.
   Assuming your system uses reasonably sized stdio buffers (1K or
   more), this routine should be able to remove the body in large
   (>500 byte) chunks.  The makes the cost of a call to "memmove"
   small but there is a premium on checking for the eom in packed
   maildrops.  The eom pattern is always a simple string so we can
   construct an efficient pattern matcher for it (e.g., a Vax "matchc"
   instruction).  Some thought went into recognizing the start of
   an eom that has been split across two buffers.

   This routine wants to deal with large chunks of data so, rather
   than "getc" into a local buffer, it uses stdio's buffer.  If
   you try to use it on a non-buffered file, you'll get what you
   deserve.  This routine "knows" that struct FILEs have a _ptr
   and a _cnt to describe the current state of the buffer and
   it knows that _filbuf ignores the _ptr & _cnt and simply fills
   the buffer.  If stdio on your system doesn't work this way, you
   may have to make small changes in this routine.

   This routine also "knows" that an EOF indication on a stream is
   "sticky" (i.e., you will keep getting EOF until you reposition the
   stream).  If your system doesn't work this way it is broken and you
   should complain to the vendor.  As a consequence of the sticky
   EOF, this routine will never return any kind of EOF status when
   there is data in "name" or "buf").
  */

/*
 * static prototypes
 */
static int m_Eom (m_getfld_state_t);

#define eom(c,s)	(s->msg_style != MS_DEFAULT && \
			 ((c) == *s->msg_delim && m_Eom(s)))

/*
 * Maildrop styles
 */
#define	MS_DEFAULT	0	/* default (one msg per file) */
#define	MS_UNKNOWN	1	/* type not known yet         */
#define	MS_MBOX		2	/* Unix-style "from" lines    */
#define	MS_MMDF		3	/* string MMDF_DELIM          */

/* This replaces the old approach, with its direct access to stdio
 * internals.  It uses one fread() to load a buffer that we manage.
 *
 * MSG_INPUT_SIZE is the size of the buffer.
 * MAX_DELIMITER_SIZE is the maximum size of the delimiter used to
 * separate messages in a maildrop, such as mbox "From ".
 *
 * Some of the tests in the test suite assume a MSG_INPUT_SIZE
 * of 8192.
 */
#define MSG_INPUT_SIZE NMH_BUFSIZ
#define MAX_DELIMITER_SIZE 5

struct m_getfld_state {
    /* Holds content of iob. */
    char msg_buf[2 * MSG_INPUT_SIZE + MAX_DELIMITER_SIZE];
    /* Points to the next byte to read from msg_buf. */
    char *readpos;
    /* Points to just after the last valid byte in msg_buf.  If readpos
     * equals end then msg_buf is empty. */
    char *end;

    /* Bytes of iob consumed during this call. */
    off_t bytes_read;
    /* Position in iob given what's been consumed ready for returning to
     * the caller.  Further than this may have been read into msg_buf. */
    off_t total_bytes_read;
    /* What fseeko(3) tells us iob's position is having just explicitly
     * set it to total_bytes_read.  Surely always the same? */
    off_t last_caller_pos;
    /* Saved position in iob from filling msg_buf, prior to returning. */
    off_t last_internal_pos;
    /* The file to read from;  I/O block.  Caller keeps passing it after
     * initialisation due to historic interface so it keeps getting
     * updated, presumably to the same value. */
    FILE *iob;

    /* Maps all the bytes of msg_delim, apart from the last two,
     * including the NUL, onto the last position in msg_delim where they
     * occur.  Bytes not present are NULL. */
    char **pat_map;
    /* One of the MS_* macros tracking the type of iob's content and
     * thus if it's a single email, or several with delimeters.  Default
     * is MS_DEFAULT. */
    int msg_style;
    /*
     * The "full" delimiter string for a packed maildrop consists
     * of a newline followed by the actual delimiter.  E.g., the
     * full string for a Unix maildrop would be: "\n\nFrom ".
     * "fdelim" points to the start of the full string and is used
     * in the BODY case of the main routine to search the buffer for
     * a possible eom.  Msg_delim points to the first character of
     * the actual delim. string (i.e., fdelim+1).  edelim
     * points to the 2nd character of actual delimiter string.  It
     * is used in m_Eom because the first character of the string
     * has been read and matched before m_Eom is called.
     */

    /* The message delimeter if iob has multiple emails, else NULL.  For
     * MS_MBOX it's the string that separates two emails, "\nFrom ",
     * i.e. the terminating blank line of the previous email, and the
     * starting From_ line of the next, but for MS_MMDF it's
     * "\001\001\001\001\n" that may start or terminate an email. */
    char *msg_delim;
    /* When searching for msg_delim after an email, it's only of
     * interest at the start of the line, i.e. when preceded by a
     * linefeed.  fdelim points to msg_delim[-1] that contains '\n' so
     * it can be used as the needle. */
    char *fdelim;
    /* The last non-NUL char of msg_delim. */
    char *delimend;
    /* strlen(fdelim). */
    int fdelimlen;
    /* The second char of msg_delim.  Used when the first char has
     * already been matched to test the rest. */
    char *edelim;
    /* strlen(edelim). */
    int edelimlen;
    /* The relationship between all of these pointers and lengths for
     * the two possible msg_delim values.
     *
     *     "\0\n\nFrom \0"   9              "\0\n\001\001\001\001\n\0"   8
     *         | ||   |                         |   |   |         |
     *         | ||   s->delimend               |   |   |         s->delimend
     *         | ||                             |   |   |
     *         | |s->edelim  s->edelimlen=5     |   |   s->edelim  s->edelimlen=4
     *         | |                              |   |
     *         | s->msg_delim                   |   s->msg_delim
     *         |                                |
     *         s->fdelim  s->fdelimlen=7        s->fdelim  s->fdelimlen=6
     */

    /* The parser's current state.  Also returned to the caller, amongst
     * other possible values, to indicate the token consumed.  One of
     * FLD, FLDPLUS, BODY, or FILEEOF. */
    int state;
    /* Whether the caller intends to ftell(3)/fseek(3) iob's position,
     * and thus whether m_getfld() needs to detect that and compensate. */
    int track_filepos;
};

static
void
m_getfld_state_init (m_getfld_state_t *gstate, FILE *iob) {
    m_getfld_state_t s;

    NEW(s);
    *gstate = s;
    s->readpos = s->end = s->msg_buf;
    s->bytes_read = s->total_bytes_read = 0;
    s->last_caller_pos = s->last_internal_pos = 0;
    s->iob = iob;
    s->pat_map = NULL;
    s->msg_style = MS_DEFAULT;
    s->msg_delim = "";
    s->fdelim = s->delimend = s->edelim = NULL;
    s->fdelimlen = s->edelimlen = 0;
    s->state = FLD;
    s->track_filepos = 0;
}

/* scan() needs to force an initial state of FLD for each message. */
void
m_getfld_state_reset (m_getfld_state_t *gstate) {
    if (*gstate) {
	(*gstate)->state = FLD;
    }
}

/* If the caller interleaves ftell*()/fseek*() calls with m_getfld()
   calls, m_getfld() must keep track of the file position.  The caller
   must use this function to inform m_getfld(). */
void
m_getfld_track_filepos (m_getfld_state_t *gstate, FILE *iob) {
    if (! *gstate) {
	m_getfld_state_init (gstate, iob);
    }

    (*gstate)->track_filepos = 1;
}

void m_getfld_state_destroy (m_getfld_state_t *gstate) {
    m_getfld_state_t s = *gstate;

    if (s) {
	if (s->fdelim) {
	    free (s->fdelim-1);
	    free (s->pat_map);
	}
	free (s);
	*gstate = 0;
    }
}

/*
  Summary of file and message input buffer positions:

  input file      -------------------------------------------EOF
                                 |              |
                          last_caller_pos  last_internal_pos


  msg_buf                   --------------------EOF
                            |         |         |
                         msg_buf   readpos     end

                            |<>|=retained characters, difference
                                 between last_internal_pos and
                                 first readpos value after reading
                                 in new chunk in read_more()

  When returning from m_getfld()/m_unknown():
  1) Save the internal file position in last_internal_pos.  That's the
     m_getfld() position reference in the input file.
  2) Set file stream position so that callers can use ftell().

  When entering m_getfld()/m_unknown():
  Check to see if the call had changed the file position.  If so,
  adjust the internal position reference accordingly.  If not, restore
  the internal file position from last_internal_pos.
*/


static void
enter_getfld (m_getfld_state_t *gstate, FILE *iob) {
    m_getfld_state_t s;
    off_t pos;
    off_t pos_movement;

    if (! *gstate) {
	m_getfld_state_init (gstate, iob);
    }
    s = *gstate;
    s->bytes_read = 0;

    /* This is ugly and no longer necessary, but is retained just in
       case it's needed again.  The parser used to open the input file
       multiple times, so we had to always use the FILE * that's
       passed to m_getfld().  Now the parser inits a new
       m_getfld_state for each file.  See comment below about the
       readpos shift code being currently unused. */
    s->iob = iob;

    if (!s->track_filepos)
        return;

    pos = ftello(iob);
    if (pos == 0 && s->last_internal_pos == 0)
        return;

    if (s->last_internal_pos == 0) {
        s->total_bytes_read = pos;
        return;
    }

    pos_movement = pos - s->last_caller_pos; /* Can be < 0. */
    if (pos_movement == 0) {
        pos = s->last_internal_pos;
    } else {
        /* The current file stream position differs from the
           last one, so caller must have called ftell/o().
           Or, this is the first call and the file position
           was not at 0. */

        if (s->readpos + pos_movement >= s->msg_buf  &&
            s->readpos + pos_movement < s->end) {
            /* This is currently unused.  It could be used by
               parse_mime() if it was changed to use a global
               m_getfld_state. */
            /* We can shift readpos and remain within the
               bounds of msg_buf. */
            s->readpos += pos_movement;
            s->total_bytes_read += pos_movement;
            pos = s->last_internal_pos;
        } else {
            size_t num_read;

            /* This seek skips past an integral number of
               chunks of size MSG_INPUT_SIZE. */
            fseeko (iob, pos/MSG_INPUT_SIZE * MSG_INPUT_SIZE, SEEK_SET);
            num_read = fread (s->msg_buf, 1, MSG_INPUT_SIZE, iob);
            s->readpos = s->msg_buf  +  pos % MSG_INPUT_SIZE;
            s->end = s->msg_buf + num_read;
            s->total_bytes_read = pos;
        }
    }

    fseeko (iob, pos, SEEK_SET);
}

static void
leave_getfld (m_getfld_state_t s) {
    s->total_bytes_read += s->bytes_read;

    if (s->track_filepos) {
	/* Save the internal file position that we use for the input buffer. */
	s->last_internal_pos = ftello (s->iob);

	/* Set file stream position so that callers can use ftell(). */
	fseeko (s->iob, s->total_bytes_read, SEEK_SET);
	s->last_caller_pos = ftello (s->iob);
    }
}

static size_t
read_more (m_getfld_state_t s) {
    /* Retain at least edelimlen characters that have already been read,
       if at least edelimlen have been read, so that we can back up to them
       in m_Eom(). */
    ssize_t retain = s->end - s->msg_buf < s->edelimlen ? 0 : s->edelimlen;
    size_t num_read;

    if (retain > 0) {
        if (retain < s->end - s->readpos)
            retain = s->end - s->readpos;
        assert (retain <= s->readpos - s->msg_buf);

        /* Move what we want to retain at end of the buffer to the beginning. */
        memmove (s->msg_buf, s->readpos - retain, retain);
    }

    s->readpos = s->msg_buf + retain;
    num_read = fread (s->readpos, 1, MSG_INPUT_SIZE, s->iob);
    s->end = s->readpos + num_read;

    return num_read;
}

/* Return the next character consumed from the input, fetching more of
 * the input for the buffer if required, or EOF on end of file. */
static int
Getc (m_getfld_state_t s) {
    if ((s->end - s->readpos < 1 && read_more (s) == 0) ||
        s->readpos >= s->end)
        return EOF;

    s->bytes_read++;
    return (unsigned char)*s->readpos++;
}

/* Return the next character that would be read by Getc() without
 * consuming it, fetching more of the input for the buffer if required,
 * or EOF on end of file. */
static int
Peek (m_getfld_state_t s) {
    if (s->end - s->readpos < 1  &&  read_more (s) == 0) {
        return EOF;
    }
    return s->readpos < s->end  ?  (unsigned char) *s->readpos  :  EOF;
}

/* If there's room, put non-EOF c back into msg_buf and rewind so it's
 * read next.  c need not be the value already in the buffer.  If there
 * isn't room then return EOF, else return c. */
static int
Ungetc (int c, m_getfld_state_t s) {
    if (s->readpos == s->msg_buf) {
	return EOF;
    }
    --s->bytes_read;
    return *--s->readpos = (unsigned char) c;
}


int
m_getfld (m_getfld_state_t *gstate, char name[NAMESZ], char *buf, int *bufsz,
          FILE *iob)
{
    m_getfld_state_t s;
    char *cp;
    int max, n, c;

    enter_getfld (gstate, iob);
    s = *gstate;

    if ((c = Getc(s)) == EOF) {
	*bufsz = *buf = 0;
	leave_getfld (s);
	return s->state = FILEEOF;
    }
    if (eom (c, s)) {
	/* flush null messages */
	while ((c = Getc(s)) != EOF && eom (c, s))
	    ;

	if (c != EOF)
	    Ungetc(c, s);
	*bufsz = *buf = 0;
	leave_getfld (s);
	return s->state = FILEEOF;
    }

    switch (s->state) {
	case FLD:
	    if (c == '\n' || c == '-') {
		/* we hit the header/body separator */
		while (c != '\n' && (c = Getc(s)) != EOF)
                    ;

		if (c == EOF || (c = Getc(s)) == EOF || eom (c, s)) {
		    /* flush null messages */
		    while ((c = Getc(s)) != EOF && eom (c, s))
			;
		    if (c != EOF)
			Ungetc(c, s);
		    *bufsz = *buf = 0;
		    leave_getfld (s);
		    return s->state = FILEEOF;
		}
		s->state = BODY;
		goto body;
	    }
	    /*
	     * get the name of this component.  take characters up
	     * to a ':', a newline or NAMESZ-1 characters, whichever
	     * comes first.
	     */
	    cp = name;
	    max = NAMESZ - 1;
	    /* Get the field name.  The first time through the loop,
	       this copies out the first character, which was loaded
	       into c prior to loop entry.  Initialize n to 1 to
	       account for that. */
	    for (n = 1;
		 c != ':'  &&  c != '\n'  &&  c != EOF  &&  n < max;
		 ++n, c = Getc (s)) {
		*cp++ = c;
	    }

	    /* Check for next character, which is either the space after
	       the ':' or the first folded whitespace. */
	    {
		int next_char;
		if (c == EOF  ||  (next_char = Peek (s)) == EOF) {
		    *bufsz = *cp = *buf = 0;
		    inform("eof encountered in field \"%s\"", name);
		    leave_getfld (s);
		    return s->state = FMTERR;
		}
	    }

	    /* If c isn't ':' here, something went wrong.  Possibilities are:
	     *  . hit a newline (error)
	     *  . got more than namesz chars. (error)
	     */
	    if (c == ':') {
		/* Finished header name, fall through to FLDPLUS below. */
	    } else if (c == '\n') {
		/* We hit the end of the line without seeing ':' to
		 * terminate the field name.  This is usually (always?)
		 * spam.  But, blowing up is lame, especially when
		 * scan(1)ing a folder with such messages.  Pretend such
		 * lines are the first of the body (at least mutt also
		 * handles it this way). */

		/* See if buf can hold this line, since we were assuming
		 * we had a buffer of NAMESZ, not bufsz. */
		/* + 1 for the newline */
		if (*bufsz < n + 1) {
		    /* No, it can't.  Oh well, guess we'll blow up. */
		    *bufsz = *cp = *buf = 0;
		    inform("eol encountered in field \"%s\"", name);
		    s->state = FMTERR;
		    break;
		}
		memcpy (buf, name, n - 1);
		buf[n - 1] = '\n';
		buf[n] = '\0';
                /* Indicate this wasn't a header field using a character
                   that can't appear in a header field. */
                name[0] = ':';
		/* The last character read was '\n'.  s->bytes_read
		   (and n) include that, but it was not put into the
		   name array in the for loop above.  So subtract 1. */
		*bufsz = --s->bytes_read;  /* == n - 1 */
		leave_getfld (s);
		return s->state = BODY;
	    }
            if (max <= n) {
		/* By design, the loop above discards the last character
                   it had read.  It's in c, use it. */
		*cp++ = c;
		*bufsz = *cp = *buf = 0;
		inform("field name \"%s\" exceeds %d bytes", name,
			NAMESZ - 2);
		s->state = LENERR;
		break;
	    }

	    /* Trim any trailing spaces from the end of name. */
	    while (isspace ((unsigned char) *--cp) && cp >= name) continue;
	    *++cp = 0;
	    /* readpos points to the first character of the field body. */
	    /* FALLTHRU */

	case FLDPLUS: {
	    /*
	     * get (more of) the text of a field.  Take
	     * characters up to the end of this field (newline
	     * followed by non-blank) or bufsz-1 characters.
	     */
	    int finished;

	    cp = buf;
	    max = *bufsz-1;
	    n = 0;
	    for (finished = 0; ! finished; ) {
		while (c != '\n'  &&  c != EOF  &&  n++ < max) {
		    if ((c = Getc (s)) != EOF)
                        *cp++ = c;
		}

		if (c != EOF)
                    c = Peek (s);
		if (max < n) {
		    /* The dest buffer is full.  Need to back the read
		       pointer up by one because when m_getfld() is
		       reentered, it will read a character.  Then
		       we'll jump right to the FLDPLUS handling code,
		       which will not store that character, but
		       instead move on to the next one. */
		    if (s->readpos > s->msg_buf) {
			--s->readpos;
			--s->bytes_read;
		    }
		    s->state = FLDPLUS;
		    finished = 1;
		} else if (c != ' '  &&  c != '\t') {
		    /* The next character is not folded whitespace, so
		       prepare to move on to the next field.  It's OK
		       if c is EOF, it will be handled on the next
		       call to m_getfld (). */
		    s->state = FLD;
		    finished = 1;
		} else {
		    /* Folded header field, continues on the next line. */
		}
	    }
	    *bufsz = s->bytes_read;
	    break;
        }

	body:
	case BODY: {
	    /*
	     * get the message body up to bufsz characters or the
	     * end of the message.
	     */
	    char *bp;

            name[0] = '\0';
	    max = *bufsz-1;
	    /* Back up and store the current position. */
	    bp = --s->readpos;
            c = min(s->end - s->readpos, max);
	    if (s->msg_style != MS_DEFAULT && c > 1) {
		/*
		 * packed maildrop - only take up to the (possible)
		 * start of the next message.  This "matchc" should
		 * probably be a Boyer-Moore matcher for non-vaxen,
		 * particularly since we have the alignment table
		 * all built for the end-of-buffer test (next).
		 * But our vax timings indicate that the "matchc"
		 * instruction is 50% faster than a carefully coded
		 * B.M. matcher for most strings.  (So much for elegant
		 * algorithms vs. brute force.)  Since I (currently)
		 * run MH on a vax, we use the matchc instruction. --vj
		 */
		char *ep;

                if ((ep = memmem(bp, c, s->fdelim, s->fdelimlen)))
                    /* Plus one to nab the '\n' that starts fdelim as
                     * that ends the previous line;  it isn't part of
                     * msg_delim. */
		    c = ep - bp + 1;
		else {
		    /*
		     * There's no delim in the buffer but there may be
		     * a partial one at the end.  If so, we want to leave
		     * it so the "eom" check on the next call picks it up.
		     * Use a modified Boyer-Moore matcher to make this
		     * check relatively cheap.  The first "if" figures
		     * out what position in the pattern matches the last
		     * character in the buffer.  The inner "while" matches
		     * the pattern against the buffer, backwards starting
		     * at that position.  Note that unless the buffer
		     * ends with one of the characters in the pattern
		     * (excluding the first and last), we do only one test.
		     */
		    char *sp;

                    ep = bp + c - 1; /* The last byte. */
		    if ((sp = s->pat_map[(unsigned char) *ep])) {
			do {
			    /* This if() is true unless (a) the buffer is too
			     * small to contain this delimiter prefix, or
			     * (b) it contains exactly enough chars for the
			     * delimiter prefix.
			     * For case (a) obviously we aren't going to match.
			     * For case (b), if the buffer really contained exactly
			     * a delim prefix, then the m_eom call at entry
			     * should have found it.  Thus it's not a delim
			     * and we know we won't get a match.
			     */
			    if (((sp - s->fdelim) + 2) <= c) {
				cp = sp;
				/* Unfortunately although fdelim has a preceding NUL
				 * we can't use this as a sentinel in case the buffer
				 * contains a NUL in exactly the wrong place (this
				 * would cause us to run off the front of fdelim).
				 */
				while (*--ep == *--cp)
				    if (cp < s->fdelim)
					break;
				if (cp < s->fdelim) {
				    /* we matched the entire delim prefix,
				     * so only take the buffer up to there.
				     * we know ep >= bp -- check above prevents underrun
				     */
				    c = (ep - bp) + 2;
				    break;
				}
			    }
			    /* try matching one less char of delim string */
			    ep = bp + c - 1;
			} while (--sp > s->fdelim);
		    }
		}
	    }
	    memcpy( buf, bp, c );
	    /* Advance the current position to reflect the copy out.
	       c is less than or equal to the number of bytes remaining
	       in the read buffer, so will not overrun it. */
	    s->readpos += c;
	    cp = buf + c;
	    /* Subtract 1 from c because the first character was read by
	       Getc(), and therefore already accounted for in s->bytes_read. */
	    s->bytes_read += c - 1;
	    *bufsz = s->bytes_read;
	    break;
        }

	default:
	    adios (NULL, "m_getfld() called with bogus state of %d", s->state);
    }

    *cp = 0;
    leave_getfld (s);

    return s->state;
}


void
m_unknown(m_getfld_state_t *gstate, FILE *iob)
{
    m_getfld_state_t s;
    int c;
    char text[MAX_DELIMITER_SIZE];
    char from[] = "From ";
    char *cp;
    char *delimstr;
    unsigned int i;

    enter_getfld (gstate, iob);
    s = *gstate;

/*
 * Figure out what the message delimiter string is for this
 * maildrop.  (This used to be part of m_Eom but I didn't like
 * the idea of an "if" statement that could only succeed on the
 * first call to m_Eom getting executed on each call, i.e., at
 * every newline in the message).
 *
 * If the first line of the maildrop is a Unix "From " line, we
 * say the style is MBOX and eat the rest of the line.  Otherwise
 * we say the style is MMDF and look for the delimiter string
 * specified when nmh was built (or from the mts.conf file).
 */

    s->msg_style = MS_UNKNOWN;

    for (i = 0, cp = text; i < sizeof text; ++i, ++cp) {
	if ((c = Getc (s)) == EOF) {
	    *cp = '\0';
	    break;
	}
        *cp = c;
    }

    if (i == sizeof from-1  &&  strncmp (text, "From ", sizeof from-1) == 0) {
	s->msg_style = MS_MBOX;
	delimstr = "\nFrom ";
	while ((c = Getc(s)) != EOF && c != '\n')
            ;
    } else {
	/* not a Unix style maildrop */
	s->readpos -= s->bytes_read;
	s->bytes_read = 0;
	delimstr = MMDF_DELIM;
	s->msg_style = MS_MMDF;
    }

    /*     "\nFrom \0"   7                  "\001\001\001\001\n\0"  6
     *       |                                  |
     *       delimstr   c=6                     delimstr   c=5
     */
    c = strlen (delimstr);
    s->fdelim = mh_xmalloc (c + 3); /* \0, \n, delimstr, \0 */
    *s->fdelim++ = '\0';
    *s->fdelim = '\n';
    s->fdelimlen = c + 1;
    s->msg_delim = s->fdelim+1;
    strcpy (s->msg_delim, delimstr);
    s->edelim = s->msg_delim+1;
    s->edelimlen = c - 1;
    s->delimend = s->msg_delim + s->edelimlen;
    if (s->edelimlen <= 1)
	adios (NULL, "maildrop delimiter must be at least 2 bytes");

    /*
     * build a Boyer-Moore end-position map for the matcher in m_getfld.
     * N.B. - we don't match just the first char (since it's the newline
     * separator) or the last char (since the matchc would have found it
     * if it was a real delim).
     */
    s->pat_map = (char **) mh_xcalloc (256, sizeof(char *));

    for (cp = s->fdelim + 1; cp < s->delimend; cp++ )
	s->pat_map[(unsigned char)*cp] = cp;

    if (s->msg_style == MS_MMDF) {
	/* flush extra msg hdrs */
	while ((c = Getc(s)) != EOF && eom (c, s))
	    ;
	if (c != EOF)
	    Ungetc(c, s);
    }

    leave_getfld (s);
}


/*
 * test for msg delimiter string
 */

static int
m_Eom (m_getfld_state_t s)
{
    int i;
    char text[MAX_DELIMITER_SIZE];
    char *cp;
    int adjust = 1;

    for (i = 0, cp = text; i < s->edelimlen; ++i, ++cp) {
	int c2;

	if ((c2 = Getc (s)) == EOF) {
	    *cp = '\0';
	    break;
	}
	*cp = c2;
    }

    if (i != s->edelimlen  ||
        strncmp (text, (char *)s->edelim, s->edelimlen)) {
	if (i == 0 && s->msg_style == MS_MBOX) {
	    /* the final newline in the (brain damaged) unix-format
	     * maildrop is part of the delimiter - delete it.
	     */
	    return 1;
	}

	if (i <= 2  &&  s->msg_style == MS_MBOX  &&
	    i != s->edelimlen  &&  ! strncmp(text, s->fdelim, i)) {
	    /* If all or part of fdelim appeared at the end of the file,
	       back up even more so that the bytes are included in the
	       message. */
	    adjust = 2;
	}

	/* Did not find delimiter, so restore the read position.
	   Note that on input, a character had already been read
	   with Getc().  It will be unget by m_getfld () on return. */
	s->readpos -= s->bytes_read - adjust;
	s->bytes_read = adjust;
	return 0;
    }

    if (s->msg_style == MS_MBOX) {
	int c;
	while ((c = Getc(s)) != EOF && c != '\n')
            ;
    }

    return 1;
}
