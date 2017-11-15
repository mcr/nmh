/* datetime.h -- functions for manipulating RFC 5545 date-time values
 *
 * This code is Copyright (c) 2017, by the authors of nmh.
 * See the COPYRIGHT file in the root directory of the nmh
 * distribution for complete copyright information. */

typedef struct tzdesc *tzdesc_t;

tzdesc_t load_timezones(const contentline *);
void free_timezones(tzdesc_t);
char *format_datetime(tzdesc_t, const contentline *);
