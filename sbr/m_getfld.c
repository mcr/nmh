
/*
 * m_getfld.c -- read/parse a message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/mts.h>
#include <h/utils.h>

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

   To speed things up considerably, the routine Eom() was made an auxilary
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
   "bcopy" but the routine call overhead on a Vax is too large for this
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
   (>500 byte) chunks.  The makes the cost of a call to "bcopy"
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
Purpose
=======
Reads an Internet message (RFC 5322), or one or more messages stored in a
maildrop in mbox (RFC 4155) or MMDF format, from a file stream.  Each call
to m_getfld() reads one header field, or a portion of the body, in sequence.

Inputs
======
state:  message parse state
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
void m_unknown(FILE *iob):  Determines the message delimiter string for the
  maildrop.  Called by inc, scan, and msh when reading from a maildrop file.

void m_eomsbr (int (*action)(int)):  Sets the hook to check for end of
  message in a maildrop.  Called only by msh.

Those functions save state in the State variables listed below.

State variables
===============
m_getfld() retains state internally between calls in some state variables.
These are used for detecting the end of each message when reading maildrops:
static unsigned char **pat_map
static unsigned char *fdelim
static unsigned char *delimend
static int fdelimlen
static unsigned char *edelim
static int edelimlen
char * msg_delim
int msg_style
int (*eom_action)(int)

Restrictions
============
m_getfld() is restricted to operate on one file stream at a time because of
the retained state (see "State variables" above).  Also, if m_getfld() is
used to read a file stream, then only m_getfld() should be used to read that
file stream.

Current usage
=============
The first call to m_getfld() on a file stream is with a state of FLD.
Subsequent calls provide the state returned by the previous call.
(Therefore, given the Restrictions above, the state variable could be
removed from the signature and just retained internally.)
*/

/*
 * static prototypes
 */
static int m_Eom (int, FILE *);
static unsigned char *matchc(int, char *, int, char *);

#define eom(c,iob)	(msg_style != MS_DEFAULT && \
			 (((c) == *msg_delim && m_Eom(c,iob)) ||\
			  (eom_action && (*eom_action)(c))))

static unsigned char **pat_map;

static int msg_style = MS_DEFAULT;

/*
 * The "full" delimiter string for a packed maildrop consists
 * of a newline followed by the actual delimiter.  E.g., the
 * full string for a Unix maildrop would be: "\n\nFrom ".
 * "Fdelim" points to the start of the full string and is used
 * in the BODY case of the main routine to search the buffer for
 * a possible eom.  Msg_delim points to the first character of
 * the actual delim. string (i.e., fdelim+1).  Edelim
 * points to the 2nd character of actual delimiter string.  It
 * is used in m_Eom because the first character of the string
 * has been read and matched before m_Eom is called.
 */
static char *msg_delim = "";
static unsigned char *fdelim;
static unsigned char *delimend;
static int fdelimlen;
static unsigned char *edelim;
static int edelimlen;

static int (*eom_action)(int) = NULL;

/* This replaces the old approach, which included direct access to
   stdio internals.  It uses one fread() to load a buffer that we
   manage.
   MSG_INPUT_SIZE is the size of the buffer.
   MAX_DELIMITER_SIZE is the maximum size of the delimiter used to
   separate messages in a maildrop, such as mbox "\nFrom ". */
#define MSG_INPUT_SIZE 8192
#define MAX_DELIMITER_SIZE 32
static struct m_getfld_buffer {
    unsigned char msg_buf[2 * MSG_INPUT_SIZE + MAX_DELIMITER_SIZE];
    unsigned char *readpos;
    unsigned char *end;  /* One past, like C++, the last character read in. */
} m;

static void
setup_buffer (FILE *iob, struct m_getfld_buffer *m) {
    /* Rely on Restrictions that m_getfld() calls on different file
       streams are not interleaved, and no other file stream read
       methods are used.  And, the first call to m_getfld (), etc., on
       a stream always reads at least 1 byte.
       I don't think it's necessary to use ftello() because we just
       need to determine whether the current offset is 0 or not. */
    if (ftell (iob) == 0) {
	/* A new file stream, so reset the buffer state. */
	m->readpos = m->end = m->msg_buf;
    }
}

static size_t
read_more (struct m_getfld_buffer *m, FILE *iob) {
    /* Retain at least edelimlen characters that have already been
       read so that we can back up to them in m_Eom(). */
    ssize_t retain = edelimlen;
    size_t num_read;

    if (m->end > m->readpos) {
	if (retain < m->end - m->readpos) retain = m->end - m->readpos;
    }
    if (retain > m->readpos - m->msg_buf) {
	/* Should not happen:  there have been fewer characters read
	   than are remaining in the buffer. */
	retain = m->readpos - m->msg_buf;
    }

    /* Move any leftover at the end of buf to the beginning. */
    memmove (m->msg_buf, m->readpos - retain, retain);

    m->readpos = m->msg_buf + retain;

    num_read = fread (m->readpos, 1, MSG_INPUT_SIZE, iob);

    m->end = m->readpos + num_read;

    return num_read;
}

static int
Getc (FILE *iob) {
    if (m.end - m.readpos < 1) {
	if (read_more (&m, iob) == 0) {
	    /* Pretend that we read a character.  That's what stdio does. */
	    ++m.readpos;
	    return EOF;
	}
    }

    return m.readpos < m.end  ?  *m.readpos++  :  EOF;
}

static int
Peek (FILE *iob) {
    int next_char = Getc (iob);
    --m.readpos;

    return next_char;
}

static int
Ungetc (int c, FILE *iob) {
    NMH_UNUSED (iob);

    return m.readpos == m.msg_buf  ?  EOF  :  (*--m.readpos = c);
}


int
m_getfld (int state, unsigned char name[NAMESZ], unsigned char *buf,
          int *bufsz, FILE *iob)
{
    register unsigned char  *bp, *cp, *ep, *sp;
    register int cnt, c, i, j;
    long bytes_read = 0;

    setup_buffer (iob, &m);

    if ((c = Getc(iob)) < 0) {
	*bufsz = *buf = 0;
	return FILEEOF;
    }
    if (eom (c, iob)) {
	if (! eom_action) {
	    /* flush null messages */
	    while ((c = Getc(iob)) >= 0 && eom (c, iob))
		;

	    if (c >= 0)
		Ungetc(c, iob);
	}
	*bufsz = *buf = 0;
	return FILEEOF;
    }

    switch (state) {
	case FLDEOF:
	case BODYEOF:
	case FLD:
	    if (c == '\n' || c == '-') {
		/* we hit the header/body separator */
		while (c != '\n' && (c = Getc(iob)) >= 0) {
		    ++bytes_read;
		}

		if (c < 0 || (c = Getc(iob)) < 0 || eom (c, iob)) {
		    if (! eom_action) {
			/* flush null messages */
			while ((c = Getc(iob)) >= 0 && eom (c, iob))
			    ;
			if (c >= 0)
			    Ungetc(c, iob);
		    }
		    *bufsz = *buf = 0;
		    return FILEEOF;
		}
		state = BODY;
		goto body;
	    }
	    /*
	     * get the name of this component.  take characters up
	     * to a ':', a newline or NAMESZ-1 characters, whichever
	     * comes first.
	     */
	    cp = name;
	    i = NAMESZ - 1;
	    for (;;) {
		/* Get the field name.  The first time through the
		   loop, this copies out the first character, which
		   was loaded into c prior to loop entry.  Initialize
		   j to 1 to account for that. */
		for (j = 1;
		     c != ':'  &&  c != '\n'  &&  c != EOF  &&  j <= i;
		     ++j, ++bytes_read, c = Getc (iob)) {
		    *cp++ = c;
		}

		/* Skip next character, which is either the space
		   after the ':' or the first folded whitespace. */
		{
		    int next_char;
		    if (c == EOF  ||  (next_char = Peek (iob)) == EOF) {
		        *bufsz = *cp = *buf = 0;
		        advise (NULL, "eof encountered in field \"%s\"", name);
		        return FMTERR;
		    }
		}
		if (c == ':')
		    break;

		/*
		 * something went wrong.  possibilities are:
		 *  . hit a newline (error)
		 *  . got more than namesz chars. (error)
		 *  . hit the end of the buffer. (loop)
		 */
		if (c == '\n') {
		    /* We hit the end of the line without seeing ':' to
		     * terminate the field name.  This is usually (always?)
		     * spam.  But, blowing up is lame, especially when
		     * scan(1)ing a folder with such messages.  Pretend such
		     * lines are the first of the body (at least mutt also
		     * handles it this way). */

		    /* See if buf can hold this line, since we were assuming
		     * we had a buffer of NAMESZ, not bufsz. */
		    /* + 1 for the newline */

		    if (*bufsz < j + 1) {
			/* No, it can't.  Oh well, guess we'll blow up. */
			*bufsz = *cp = *buf = 0;
			advise (NULL, "eol encountered in field \"%s\"", name);
			state = FMTERR;
			goto finish;
		    }
		    memcpy (buf, name, j - 1);
		    buf[j - 1] = '\n';
		    buf[j] = '\0';
		    *bufsz = bytes_read;
		    return BODY;
		}
		if ((i -= j) <= 0) {
		    *bufsz = *cp = *buf = 0;
		    advise (NULL, "field name \"%s\" exceeds %d bytes", name, NAMESZ - 2);
		    state = LENERR;
		    goto finish;
		}
	    }

	    /* Trim any trailing spaces from the end of name. */
	    while (isspace (*--cp) && cp >= name) continue;
	    *++cp = 0;
	    /* c is ':' here.  And readpos points to the first
	       character of the field body. */
	    /* fall through */

	case FLDPLUS:
	    /*
	     * get (more of) the text of a field.  Take
	     * characters up to the end of this field (newline
	     * followed by non-blank) or bufsz-1 characters.
	     */
	    cp = buf;
	    i = *bufsz-1;
	    for (;;) {
		for (j = 0; c != '\n'  &&  c != EOF  &&  j++ <= i; ) {
		    *cp++ = c = Getc (iob);
		}

		if (c != EOF) c = Peek (iob);
		if (i < j) {
		    /* the dest buffer is full */
		    state = FLDPLUS;
		    goto finish;
		} else if (c != ' '  &&  c != '\t') {
		    /* The next character is not folded whitespace, so
		       prepare to move on to the next field.  It's OK
		       if c is EOF, it will be handled on the next
		       call to m_getfld (). */
		    state = FLD;
		    goto finish;
		} else {
		    /* Folded header field, continues on the next line. */
		    continue;
		}
	    }
	    break;

	case BODY:
	body:
	    /*
	     * get the message body up to bufsz characters or the
	     * end of the message.
	     */
	    i = *bufsz-1;
	    /* Back up and store the current position and update cnt. */
	    bp = --m.readpos;
	    cnt = m.end - m.readpos;
	    c = cnt < i ? cnt : i;
	    if (msg_style != MS_DEFAULT && c > 1) {
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
		if ((ep = matchc( fdelimlen, fdelim, c, bp )))
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
		    ep = bp + c - 1;
		    if ((sp = pat_map[*ep])) {
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
			    if (((sp - fdelim) + 2) <= c) {
				cp = sp;
				/* Unfortunately although fdelim has a preceding NUL
				 * we can't use this as a sentinel in case the buffer
				 * contains a NUL in exactly the wrong place (this
				 * would cause us to run off the front of fdelim).
				 */
				while (*--ep == *--cp)
				    if (cp < fdelim)
					break;
				if (cp < fdelim) {
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
			} while (--sp > fdelim);
		    }
		}
	    }
	    memcpy( buf, bp, c );
	    /* Advance the current position to reflect the copy out.
	       c is less than or equal to the number of bytes remaining
	       in the read buffer, so will not overrun it. */
	    m.readpos += c;
	    cp = buf + c;
	    break;

	default:
	    adios (NULL, "m_getfld() called with bogus state of %d", state);
    }
finish:
    *cp = 0;
    *bufsz = cp - buf + bytes_read + 1;
    return (state);
}


void
m_unknown(FILE *iob)
{
    register int c;
    char text[MAX_DELIMITER_SIZE];
    register char *cp;
    register char *delimstr;
    size_t bytes_read = 0;

    setup_buffer (iob, &m);

/*
 * Figure out what the message delimitter string is for this
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

    msg_style = MS_UNKNOWN;

    for (c = 0, cp = text; c < 5; ++c, ++cp) {
	++bytes_read;
	if ((*cp = Getc (iob)) == EOF) {
	    break;
	}
    }

    if (c == 5  &&  strncmp (text, "From ", 5) == 0) {
	msg_style = MS_MBOX;
	delimstr = "\nFrom ";
	while (++bytes_read, (c = Getc (iob)) != '\n' && c >= 0)
	    ;
	/* m_unknown is only called on maildrop files, and they are only
	   read using m_getfld ().  The caller musn't try to read from
	   the stream directly because the file position indicator was
	   not advanced based on bytes_read, but instead on whatever
	   was read into the message buffer. */
    } else {
	/* not a Unix style maildrop */
	m.readpos -= bytes_read; /* fseek (iob, pos, SEEK_SET), but relative. */
	if (mmdlm2 == NULL || *mmdlm2 == 0)
	    mmdlm2 = "\001\001\001\001\n";
	delimstr = mmdlm2;
	msg_style = MS_MMDF;
    }
    c = strlen (delimstr);
    fdelim = (unsigned char *) mh_xmalloc((size_t) (c + 3));
    *fdelim++ = '\0';
    *fdelim = '\n';
    msg_delim = (char *)fdelim+1;
    edelim = (unsigned char *)msg_delim+1;
    fdelimlen = c + 1;
    edelimlen = c - 1; /* == strlen (delimstr) */
    strcpy (msg_delim, delimstr);
    delimend = (unsigned char *)msg_delim + edelimlen;
    if (edelimlen <= 1)
	adios (NULL, "maildrop delimiter must be at least 2 bytes");
    /*
     * build a Boyer-Moore end-position map for the matcher in m_getfld.
     * N.B. - we don't match just the first char (since it's the newline
     * separator) or the last char (since the matchc would have found it
     * if it was a real delim).
     */
    pat_map = (unsigned char **) calloc (256, sizeof(unsigned char *));

    for (cp = (char *) fdelim + 1; cp < (char *) delimend; cp++ )
	pat_map[(unsigned char)*cp] = (unsigned char *) cp;

    if (msg_style == MS_MMDF) {
	/* flush extra msg hdrs */
	while ((c = Getc(iob)) >= 0 && eom (c, iob))
	    ;
	if (c >= 0)
	    Ungetc(c, iob);
    }
}


void
m_eomsbr (int (*action)(int))
{
    if ((eom_action = action)) {
	msg_style = MS_MSH;
	*msg_delim = 0;
	fdelimlen = 1;
	delimend = fdelim;
    } else {
	msg_style = MS_MMDF;
	msg_delim = (char *)fdelim + 1;
	fdelimlen = strlen((char *)fdelim);
	delimend = (unsigned char *)(msg_delim + edelimlen);
    }
}


/*
 * test for msg delimiter string
 */

static int
m_Eom (int c, FILE *iob)
{
    register int i;
    size_t bytes_read = 0;
    char text[MAX_DELIMITER_SIZE];
    char *cp;

    for (i = 0, cp = text; i < edelimlen; ++i, ++cp) {
	++bytes_read;
	if ((*cp = Getc (iob)) == EOF) {
	    break;
	}
    }

    if (i != edelimlen  ||  strncmp (text, (char *)edelim, edelimlen)) {
	if (i == 0 && msg_style == MS_MBOX)
	    /* the final newline in the (brain damaged) unix-format
	     * maildrop is part of the delimitter - delete it.
	     */
	    return 1;

	/* Did not find delimiter, so restore the read position.
	   Note that on input, a character had already been read
	   with Getc().  It will be unget by m_getfld () on return. */
	m.readpos -= bytes_read;
	return 0;
    }

    if (msg_style == MS_MBOX) {
	while ((c = Getc (iob)) != '\n')
	    if (c < 0)
		break;
    }

    return 1;
}


static unsigned char *
matchc(int patln, char *pat, int strln, char *str)
{
	register char *es = str + strln - patln;
	register char *sp;
	register char *pp;
	register char *ep = pat + patln;
	register char pc = *pat++;

	for(;;) {
		while (pc != *str++)
			if (str > es)
				return 0;
		if (str > es+1)
			return 0;
		sp = str; pp = pat;
		while (pp < ep && *sp++ == *pp)
			pp++;
		if (pp >= ep)
			return ((unsigned char *)--str);
	}
}
