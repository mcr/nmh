/* ruserpass.h -- parse .netrc-format file.
 *
 * Portions of this code are
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Portions of this code are Copyright (c) 2017, by the authors of
 * nmh.  See the COPYRIGHT file in the root directory of the nmh
 * distribution for complete copyright information. */

/*
 * Read our credentials file and (optionally) ask the user for anything
 * missing.
 *
 * Arguments:
 *
 * host		- Hostname (to scan credentials file)
 * aname	- Pointer to filled-in username
 * apass	- Pointer to filled-in password
 * flags	- One or more of RUSERPASS_NO_PROMPT_USER,
 *                RUSERPASS_NO_PROMPT_PASSWORD
 */
void ruserpass(const char *, char **, char **, int);

#define RUSERPASS_NO_PROMPT_USER	0x01
#define RUSERPASS_NO_PROMPT_PASSWORD	0x02
