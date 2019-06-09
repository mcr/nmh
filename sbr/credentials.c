/* credentials.c -- wrap configurable access to .netrc or similar files.
 *
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "concat.h"
#include "ruserpass.h"
#include "credentials.h"
#include "context_find.h"
#include "error.h"
#include "h/utils.h"
#include "h/mts.h"
#include "m_maildir.h"

struct nmh_creds {
    char *host;		/* Hostname corresponding to credentials */
    char *user;		/* Username corresponding to credentials */
    char *pass;		/* (Optional) password used by credentials */
};

void
init_credentials_file(void)
{
    if (credentials_file == NULL) {
        char *cred_style = context_find ("credentials");

        if (cred_style == NULL  ||  ! strcmp (cred_style, "legacy")) {
            char *hdir = getenv ("HOME");

            credentials_file = concat (hdir ? hdir : ".", "/.netrc", NULL);
        } else if (! strncasecmp (cred_style, "file:", 5) ||
		   ! strncasecmp (cred_style, "file-nopermcheck:", 17)) {
            struct stat st;
            char *filename = strchr(cred_style, ':') + 1;

            while (isspace((unsigned char)*filename))
                filename++;

            if (*filename == '/') {
                credentials_file = filename;
            } else {
                credentials_file = m_maildir (filename);
                if (stat (credentials_file, &st) != OK) {
                    credentials_file =
                        concat (mypath ? mypath : ".", "/", filename, NULL);
                    if (stat (credentials_file, &st) != OK) {
                        inform("unable to find credentials file %s, continuing...",
                                  filename);
                    }
                }
            }

	    if (! strncasecmp (cred_style, "file-nopermcheck:", 17))
		credentials_no_perm_check = 1;
        }
    }
}

nmh_creds_t
nmh_get_credentials (const char *host, const char *user)
{
    nmh_creds_t creds;

    char *cred_style = context_find ("credentials");

    init_credentials_file ();

    creds = mh_xmalloc(sizeof(*creds));

    creds->host = mh_xstrdup(host);
    creds->user = NULL;
    creds->pass = NULL;

    if (cred_style == NULL  ||  ! strcmp (cred_style, "legacy")) {
	creds->user = user == NULL  ?  mh_xstrdup(getusername (1))  :  mh_xstrdup(user);
    } else if (! strncasecmp (cred_style, "file:", 5) ||
	       ! strncasecmp (cred_style, "file-nopermcheck:", 17)) {
        /*
         * Determine user using the first of:
         * 1) -user switch
         * 2) matching host entry with login in a credentials file
         *    such as ~/.netrc
         * 3) interactively request from user (as long as the
         *    credentials file didn't have a "default" token)
         */
        creds->user = user == NULL ? NULL : mh_xstrdup(user);
    } else {
        inform("unknown credentials style %s, continuing...", cred_style);
        return NULL;
    }

    ruserpass(creds->host, &creds->user, &creds->pass,
	      RUSERPASS_NO_PROMPT_USER | RUSERPASS_NO_PROMPT_PASSWORD);

    return creds;
}

/*
 * Retrieve the username
 */

const char *
nmh_cred_get_user(nmh_creds_t creds)
{
    if (! creds->user) {
	ruserpass(creds->host, &creds->user, &creds->pass,
		  RUSERPASS_NO_PROMPT_PASSWORD);
    }

    return creds->user;
}

/*
 * Retrieve the password
 */

const char *
nmh_cred_get_password(nmh_creds_t creds)
{
    if (! creds->pass) {
	ruserpass(creds->host, &creds->user, &creds->pass, 0);
    }

    return creds->pass;
}

/*
 * Free our credentials
 */

void
nmh_credentials_free(nmh_creds_t creds)
{
    free(creds->host);
    free(creds->user);

    if (creds->pass) {
	memset(creds->pass, 0, strlen(creds->pass));
	free(creds->pass);
    }

    free(creds);
}
