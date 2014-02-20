/*
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/mts.h>

void
init_credentials_file () {
    if (credentials_file == NULL) {
        char *cred_style = context_find ("credentials");

        if (cred_style == NULL  ||  ! strcmp (cred_style, "legacy")) {
            char *hdir = getenv ("HOME");

            credentials_file = concat (hdir ? hdir : ".", "/.netrc", NULL);
        } else if (! strncasecmp (cred_style, "file:", 5)) {
            struct stat st;
            char *filename = cred_style + 5;

            while (*filename && isspace ((unsigned char) *filename)) ++filename;

            if (*filename == '/') {
                credentials_file = filename;
            } else {
                credentials_file = m_maildir (filename);
                if (stat (credentials_file, &st) != OK) {
                    credentials_file =
                        concat (mypath ? mypath : ".", "/", filename, NULL);
                    if (stat (credentials_file, &st) != OK) {
                        admonish (NULL, "unable to find credentials file %s",
                                  filename);
                    }
                }
            }
        }
    }
}

int
nmh_get_credentials (char *host, char *user, int sasl, nmh_creds_t creds) {
    char *cred_style = context_find ("credentials");

    init_credentials_file ();
    creds->host = host;

    if (cred_style == NULL  ||  ! strcmp (cred_style, "legacy")) {
        if (sasl) {
            /* This is what inc.c and msgchk.c used to contain. */
            /* Only inc.c and msgchk.c do this.  smtp.c doesn't. */
            creds->user = user == NULL  ?  getusername ()  :  user;
            creds->password = getusername ();
        }
    } else if (! strncasecmp (cred_style, "file:", 5)) {
        /*
         * Determine user using the first of:
         * 1) -user switch
         * 2) matching host entry with login in a credentials file
         *    such as ~/.netrc
         * 3) interactively request from user (as long as the
         *    credentials file didn't have a "default" token)
         */
        creds->user = user;
    } else {
        admonish (NULL, "unknown credentials style %s", cred_style);
        return NOTOK;
    }

    ruserpass (host, &creds->user, &creds->password);
    return OK;
}
