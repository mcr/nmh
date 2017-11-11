/* credentials.h -- wrap configurable access to .netrc or similar files.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

/*
 * credentials management
 */
void init_credentials_file(void);

/*
 * Allocate and return a credentials structure.  The credentials structure
 * is now opaque; you need to use accessors to get inside of it.  The
 * accessors will only prompt the user for missing fields if they are
 * needed.
 *
 * Arguments:
 *
 * host	- Hostname we're connecting to (used to search credentials file)
 * user	- Username we are logging in as; can be NULL.
 *
 * Returns NULL on error, otherwise an allocated nmh_creds structure.
 */
nmh_creds_t nmh_get_credentials(const char *, const char *);

/*
 * Retrieve the user from a nmh_creds structure.  May prompt the user
 * if one is not defined.
 *
 * Arguments:
 *
 * creds	- Structure from previous nmh_get_credentials() call
 *
 * Returns NULL on error, otherwise a NUL-terminated string containing
 * the username.  Points to allocated memory in the credentials structure
 * that is free()d by nmh_free_credentials().
 */
const char *nmh_cred_get_user(nmh_creds_t);

/*
 * Retrieve the password from an nmh_creds structure.  Otherwise identical
 * to nmh_cred_get_user().
 */
const char *nmh_cred_get_password(nmh_creds_t);

/*
 * Free an allocated nmh_creds structure.
 */
void nmh_credentials_free(nmh_creds_t);
