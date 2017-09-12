/* oauth_prof.c -- OAuth 2.0 implementation for XOAUTH2 in SMTP and POP3.
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#ifdef OAUTH_SUPPORT

#include <sys/stat.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <h/oauth.h>
#include <h/utils.h>
#include "m_maildir.h"

static const struct mh_oauth_service_info SERVICES[] = {
    /* https://developers.google.com/accounts/docs/OAuth2InstalledApp */
    {
        /* name */ "gmail",
        /* display_name */ "Gmail",

        /* client_id */ "91584523849-8lv9kgp1rvp8ahta6fa4b125tn2polcg.apps.googleusercontent.com",
        /* client_secret */ "Ua8sX34xyv7hVrKM-U70dKI6",

        /* auth_endpoint */ "https://accounts.google.com/o/oauth2/auth",
        /* redirect_uri */ "urn:ietf:wg:oauth:2.0:oob",
        /* token_endpoint */ "https://accounts.google.com/o/oauth2/token",
        /* scope */ "https://mail.google.com/"
    }
};

/* Copy service info so we don't have to free it only sometimes. */
static void
copy_svc(mh_oauth_service_info *to, const mh_oauth_service_info *from)
{
    to->display_name = from->display_name;
#define copy(_field_) to->_field_ = getcpy(from->_field_)
    copy(name);
    copy(scope);
    copy(client_id);
    copy(client_secret);
    copy(auth_endpoint);
    copy(token_endpoint);
    copy(redirect_uri);
#undef copy
}

/* Return profile component node name for a service parameter. */
char *
mh_oauth_node_name_for_svc(const char *base_name, const char *svc)
{
    /* TODO: s/_/-/g ? */
    return concat("oauth-", svc, "-", base_name, NULL);
}

/* Update one service_info field if overridden in profile. */
static void
update_svc_field(char **field, const char *base_name, const char *svc)
{
    char *name = mh_oauth_node_name_for_svc(base_name, svc);
    const char *value = context_find(name);
    if (value != NULL) {
        free(*field);
        *field = mh_xstrdup(value);
    }
    free(name);
}

/* Update all service_info fields that are overridden in profile. */
static bool
update_svc(mh_oauth_service_info *svc, const char *svc_name, char *errbuf,
	   size_t errbuflen)
{
#define update(name)                                                    \
    update_svc_field(&svc->name, #name, svc_name);                       \
    if (svc->name == NULL) {                                             \
    	snprintf(errbuf, errbuflen, "%s", #name " is missing");		 \
	errbuf[errbuflen - 1] = '\0';					 \
        return false;                                                    \
    }
    update(scope);
    update(client_id);
    update(client_secret);
    update(auth_endpoint);
    update(token_endpoint);
    update(redirect_uri);
#undef update

    if (svc->name == NULL) {
        svc->name = getcpy(svc_name);
    }

    if (svc->display_name == NULL) {
        svc->display_name = svc->name;
    }

    return true;
}

bool
mh_oauth_get_service_info(const char *svc_name, mh_oauth_service_info *svcinfo,
			  char *errbuf, size_t errbuflen)
{
    int i;

    svcinfo->name = svcinfo->display_name = NULL;
    svcinfo->scope = svcinfo->client_id = NULL;
    svcinfo->client_secret = svcinfo->auth_endpoint = NULL;
    svcinfo->token_endpoint = svcinfo->redirect_uri = NULL;

    for (i = 0; i < (int)DIM(SERVICES); i++) {
        if (strcmp(SERVICES[i].name, svc_name) == 0) {
            copy_svc(svcinfo, &SERVICES[i]);
            break;
        }
    }

    return update_svc(svcinfo, svc_name, errbuf, errbuflen);
}

/* Return value must be free(3)'d. */
const char *
mh_oauth_cred_fn(const char *svc)
{
    char *key = mh_oauth_node_name_for_svc("credential-file", svc);
    char *value = context_find(key);
    free(key);

    bool found = value;
    if (found) {
        if (*value == '/')
            return mh_xstrdup(value);
    } else
        value = concat("oauth-", svc, NULL);

    const char *md = m_maildir(value);
    if (!found)
        free(value);

    return mh_xstrdup(md);
}
#endif
