/* fmt_addr.h -- format an address field (from fmt_scan)
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

/*
 * The implementation of the %(formataddr) function.  This is available for
 * programs to provide their own local implementation if they wish to do
 * special processing (see uip/replsbr.c for an example).  Arguments are:
 *
 * orig		- Existing list of addresses
 * str		- New address(es) to append to list.
 *
 * This function returns an allocated string containing the new list of
 * addresses.
 */
char *formataddr(char *, char *);

/*
 * The implementation of the %(concataddr) function.  Arguments and behavior
 * are the same as %(formataddr).  Again, see uip/replsbr.c to see how you
 * can override this behavior.
 */
char *concataddr(char *, char *);
