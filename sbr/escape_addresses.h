/* escape_addresses.h -- Escape address components, hopefully per RFC 5322.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

void escape_display_name(char *, size_t);
void escape_local_part(char *, size_t);
