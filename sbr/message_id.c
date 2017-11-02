/* message_id.c -- construct the body of a Message-ID or Content-ID
 *                 header field
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include "h/mts.h"
#include "m_rand.h"
#include "message_id.h"
#include <sys/time.h>  /* for gettimeofday() */
#include "base64.h"


static enum {
  NMH_MESSAGE_ID_LOCALNAME,
  NMH_MESSAGE_ID_RANDOM
} message_id_style = NMH_MESSAGE_ID_LOCALNAME;
static char message_id_[BUFSIZ];


/* Convert name of message id style to integer value and store it. */
int
save_message_id_style (const char *value)
{
  if (! strcasecmp (value, "localname")) {
    message_id_style = NMH_MESSAGE_ID_LOCALNAME;
    return 0;
  }
  if (! strcasecmp (value, "random")) {
    message_id_style = NMH_MESSAGE_ID_RANDOM;
    return 0;
  }
  return 1;
}


char *
message_id (time_t tclock, int content_id)
{
  switch (message_id_style) {
    case NMH_MESSAGE_ID_LOCALNAME: {
#define P(fmt) \
    snprintf(message_id_, sizeof message_id_, \
        (fmt), (int)getpid(), (long)tclock, LocalName(1))

            if (content_id)
                P("<%d.%ld.%%d@%s>");
            else
                P("<%d.%ld@%s>");
#undef P
      break;
    }

    case NMH_MESSAGE_ID_RANDOM: {
      /* Use a sequence of digits divisible by 3 because that will
         expand to base64 without any waste.  Must be shorter than 58,
         see below. */
      unsigned char rnd[9];
      /* The part after the '@' is divided into thirds.  The base64
         encoded string will be 4/3 the size of rnd. */
      size_t one_third = sizeof rnd * 4/3/3;

      if (m_rand (rnd, sizeof rnd) == 0) {
        struct timeval now;
        /* All we really need is 4 * [sizeof rnd/3] + 2, as long as
           the base64 encoding stays shorter than 76 bytes so embedded
           newlines aren't necessary.  But use double the sizeof rnd
           just to be safe. */
        unsigned char rnd_base64[2 * sizeof rnd];
        unsigned char *cp;
        int i;

        writeBase64 (rnd, sizeof rnd, rnd_base64);

        for (i = strlen ((const char *) rnd_base64) - 1;
             i > 0  &&  iscntrl (rnd_base64[i]);
             --i) {
          /* Remove trailing newline.  rnd_base64 had better be
             shorter than 76 characters, so don't bother to look for
             embedded newlines. */
          rnd_base64[i] = '\0';
        }

        /* Try to make the base64 string look a little more like a
           hostname by replacing + with - and / with _. */
        for (cp = rnd_base64; *cp; ++cp) {
          if (*cp == '+') {
            *cp = '-';
          } else if (*cp == '/') {
            *cp = '_';
          }
        }

        /* gettimeofday() and getpid() shouldn't fail on POSIX platforms. */
        gettimeofday (&now, 0);

        /* The format string inserts a couple of dots, for the benefit
           of spam filters that want to see a message id with a final
           part that resembles a hostname. */
#define P(fmt) \
    snprintf(message_id_, sizeof message_id_, (fmt), \
        getpid(), (long)now.tv_sec, (long)now.tv_usec, \
        (int)one_third, rnd_base64, \
        (int)one_third, &rnd_base64[one_third], \
        (int)one_third, &rnd_base64[2*one_third])

            if (content_id)
                P("<%d-%ld.%06ld%%d@%.*s.%.*s.%.*s>");
            else
                P("<%d-%ld.%06ld@%.*s.%.*s.%.*s>");
#undef P
      }

      break;
    }
  }

  return message_id_;
}
