/*
 * message-id.c -- construct the body of a Message-ID or Content-ID
 *                 header field
 *
 * This code is Copyright (c) 2012, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <unistd.h>   /* for getpid() */
#include <sys/time.h> /* for gettimeofday() */
#include <stdio.h>


#define NMH_MESSAGE_ID_LOCALNAME 0
#define NMH_MESSAGE_ID_RANDOM 1

static int message_id_style = NMH_MESSAGE_ID_LOCALNAME;
static char message_id_[BUFSIZ];


/* Convert name of message id style to integer value and store it. */
int
save_message_id_style (const char *value) {
  if (! mh_strcasecmp (value, "localname")) {
    message_id_style = NMH_MESSAGE_ID_LOCALNAME;
    return 0;
  } else if (! mh_strcasecmp (value, "random")) {
    message_id_style = NMH_MESSAGE_ID_RANDOM;
    return 0;
  } else {
    return 1;
  }
}


char *
message_id (time_t tclock, int content_id) {
  if (message_id_style == NMH_MESSAGE_ID_LOCALNAME) {
    char *format = content_id  ?  "<%d.%ld.%%d@%s>"  :  "<%d.%ld@%s>";

    snprintf (message_id_, sizeof message_id_, format,
              (int) getpid (), (long) tclock, LocalName (1));
  } else if  (message_id_style == NMH_MESSAGE_ID_RANDOM) {
    char *format = content_id ? "<%d-%ld.%06ld%%d@%s>" : "<%d-%ld.%06ld@%s>";
    /* Use a sequence of digits divisible by 3 because that will
       expand to base64 without any waste.  Must be shorter than 58,
       see below. */
    unsigned char rnd[12];

    if (m_rand (rnd, sizeof rnd) == 0) {
      pid_t pid;
      struct timeval now;
      /* All we really need is 4 * [sizeof rnd/3] + 2, as long as the
         base64 encoding stays shorter than 76 bytes so embedded
         newlines aren't necessary.  But use double the sizeof rnd
         just to be safe. */
      unsigned char rnd_base64[2 * sizeof rnd];
      int i;

      writeBase64 (rnd, sizeof rnd, rnd_base64);

      for (i = strlen ((const char *) rnd_base64) - 1;
           i > 0  &&  iscntrl (rnd_base64[i]);
           --i) {
        /* Remove trailing newline.  rnd_base64 had better be shorter
           than 76 characters, so don't bother to look for embedded
           newlines. */
        rnd_base64[i] = '\0';
      }

      /* Neither of these can fail according to the POSIX spec. */
      pid = getpid ();
      gettimeofday (&now, 0);

      snprintf (message_id_, sizeof message_id_, format,
                pid, (long) now.tv_sec, (long) now.tv_usec, rnd_base64);
    }
  } else {
    /* Should never get here. */
    adios (0, "invalid message id style \"%s\"", message_id_style);
  }


  return message_id_;
}
