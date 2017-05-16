/* message_id.h -- construct the body of a Message-ID or Content-ID.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/* What style to use for generated Message-ID and Content-ID header
 * fields.  The localname style is pid.time@localname, where time is
 * in seconds.  The random style replaces the localname with some
 * (pseudo)random bytes and uses microsecond-resolution time. */
int save_message_id_style(const char *value);

char *message_id(time_t tclock, int content_id);
