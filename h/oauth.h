/*
 * Implementation of OAuth 2.0 [1] for XOAUTH2 in SMTP [2] and POP3 [3].
 *
 * Google defined XOAUTH2 for SMTP, and that's what we use here.  If other
 * providers implement XOAUTH2 or some similar OAuth-based SMTP authentication
 * protocol, it should be simple to extend this.
 *
 * [1] https://tools.ietf.org/html/rfc6749
 * [2] https://developers.google.com/gmail/xoauth2_protocol
 * [3] http://googleappsdeveloper.blogspot.com/2014/10/updates-on-authentication-for-gmail.html
 *
 * Presumably [2] should document POP3 and that is an over-sight.  As it stands,
 * that blog post is the closest we have to documentation.
 *
 * According to [1] 2.1 Client Types, this is a "native application", a
 * "public" client.
 *
 * To summarize the flow:
 *
 * 1. User runs mhlogin which prints a URL the user must visit, and prompts for
 *    a code retrieved from that page.
 *
 * 2. User vists this URL in browser, signs in with some Google account, and
 *    copies and pastes the resulting code back to mhlogin.
 *
 * 3. mhlogin does HTTP POST to Google to exchange the user-provided code for a
 *    short-lived access token and a long-lived refresh token.
 *
 * 4. send uses the access token in SMTP auth if not expired.  If it is expired,
 *    it does HTTP POST to Google including the refresh token and gets back a
 *    new access token (and possibly refresh token).  If the refresh token has
 *    become invalid (e.g. if the user took some reset action on the Google
 *    account), the user must use mhlogin again, then re-run send.
 */

typedef enum {
    /* error loading profile */
    MH_OAUTH_BAD_PROFILE = OK + 1,

    /* error initializing libcurl */
    MH_OAUTH_CURL_INIT,

    /* local error initializing HTTP request */
    MH_OAUTH_REQUEST_INIT,

    /* error executing HTTP POST request */
    MH_OAUTH_POST,

    /* HTTP response body is too big. */
    MH_OAUTH_RESPONSE_TOO_BIG,

    /* Can't process HTTP response body. */
    MH_OAUTH_RESPONSE_BAD,

    /* The authorization server rejected the grant (authorization code or
     * refresh token); possibly the user entered a bad code, or the refresh
     * token has become invalid, etc. */
    MH_OAUTH_BAD_GRANT,

    /* HTTP server indicates something is wrong with our request. */
    MH_OAUTH_REQUEST_BAD,

    /* Attempting to refresh an access token without a refresh token. */
    MH_OAUTH_NO_REFRESH,


    /* requested user not in cred file */
    MH_OAUTH_CRED_USER_NOT_FOUND,

    /* error loading serialized credentials */
    MH_OAUTH_CRED_FILE
} mh_oauth_err_code;

typedef struct mh_oauth_ctx mh_oauth_ctx;

typedef struct mh_oauth_cred mh_oauth_cred;

typedef struct mh_oauth_service_info mh_oauth_service_info;

struct mh_oauth_service_info {
    /* Name of service, so we can search static internal services array
     * and for determining default credential file name. */
    char *name;

    /* Human-readable name of the service; in mh_oauth_ctx::svc this is not
     * another buffer to free, but a pointer to either static SERVICE data
     * (below) or to the name field. */
    char *display_name;

    /* [1] 2.2 Client Identifier, 2.3.1 Client Password */
    char *client_id;
    /* [1] 2.3.1 Client Password */
    char *client_secret;
    /* [1] 3.1 Authorization Endpoint */
    char *auth_endpoint;
    /* [1] 3.1.2 Redirection Endpoint */
    char *redirect_uri;
    /* [1] 3.2 Token Endpoint */
    char *token_endpoint;
    /* [1] 3.3 Access Token Scope */
    char *scope;
};

/*
 * Do the complete dance for XOAUTH2 as used by POP3 and SMTP.
 *
 * Load tokens for svc from disk, refresh if necessary, and return the
 * base64-encoded client response.
 *
 * If refreshing, writes freshened tokens to disk.
 *
 * Exits via adios on any error.
 */
char *
mh_oauth_do_xoauth(const char *user, const char *svc, FILE *log);

/*
 * Allocate and initialize a new OAuth context.
 *
 * Caller must call mh_oauth_free(ctx) when finished, even on error.
 *
 * svc_name must point to a null-terminated string identifying the service
 * provider.  Support for "gmail" is built-in; anything else must be defined in
 * the user's profile.  The profile can also override "gmail" settings.
 *
 * Accesses global m_defs via context_find.
 *
 * On error, return FALSE and set an error in ctx; ctx is always allocated.
 */
boolean
mh_oauth_new(mh_oauth_ctx **ctx, const char *svc_name);

/*
 * Free all resources associated with ctx.
 */
void
mh_oauth_free(mh_oauth_ctx *ctx);

/*
 * Return null-terminated human-readable name of the service, e.g. "Gmail".
 *
 * Never returns NULL.
 */
const char *
mh_oauth_svc_display_name(const mh_oauth_ctx *ctx);

/*
 * Enable logging for subsequent operations on ctx.
 *
 * log must not be closed until after mh_oauth_free.
 *
 * For all HTTP requests, the request is logged with each line prefixed with
 * "< ", and the response with "> ".  Other messages are prefixed with "* ".
 */
void
mh_oauth_log_to(FILE *log, mh_oauth_ctx *ctx);

/*
 * Return the error code after some function indicated an error.
 *
 * Must not be called if an error was not indicated.
 */
mh_oauth_err_code
mh_oauth_get_err_code(const mh_oauth_ctx *ctx);

/*
 * Return null-terminated error message after some function indicated an error.
 *
 * Never returns NULL, but must not be called if an error was not indicated.
 */
const char *
mh_oauth_get_err_string(mh_oauth_ctx *ctx);

/*
 * Return the null-terminated URL the user needs to visit to authorize access.
 *
 * URL may be invalidated by subsequent calls to mh_oauth_get_authorize_url,
 * mh_oauth_authorize, or mh_oauth_refresh.
 *
 * On error, return NULL.
 */
const char *
mh_oauth_get_authorize_url(mh_oauth_ctx *ctx);

/*
 * Exchange code provided by the user for access (and maybe refresh) token.
 *
 * On error, return NULL.
 */
mh_oauth_cred *
mh_oauth_authorize(const char *code, mh_oauth_ctx *ctx);

/*
 * Refresh access (and maybe refresh) token if refresh token present.
 *
 * On error, return FALSE and leave cred untouched.
 */
boolean
mh_oauth_refresh(mh_oauth_cred *cred);

/*
 * Return whether access token is present and not expired at time T.
 */
boolean
mh_oauth_access_token_valid(time_t t, const mh_oauth_cred *cred);

/*
 * Free all resources associated with cred.
 */
void
mh_oauth_cred_free(mh_oauth_cred *cred);

/*
 * Return the null-terminated file name for storing this service's OAuth tokens.
 *
 * Accesses global m_defs via context_find.
 *
 * Never returns NULL.
 */
const char *
mh_oauth_cred_fn(const char *svc_name);

/*
 * Serialize OAuth tokens to file.
 *
 * On error, return FALSE.
 */
boolean
mh_oauth_cred_save(FILE *fp, mh_oauth_cred *cred, const char *user);

/*
 * Load OAuth tokens from file.
 *
 * Calls m_getfld(), which writes to stderr with advise().
 *
 * On error, return NULL.
 */
mh_oauth_cred *
mh_oauth_cred_load(FILE *fp, mh_oauth_ctx *ctx, const char *user);

/*
 * Return null-terminated SASL client response for XOAUTH2 from access token.
 *
 * Store the length in res_len.
 *
 * Must not be called except after successful mh_oauth_access_token_valid or
 * mh_oauth_refresh call; i.e. must have a valid access token.
 */
const char *
mh_oauth_sasl_client_response(size_t *res_len,
                              const char *user, const mh_oauth_cred *cred);

/*
 * Retrieve the various entries for the OAuth mechanism
 */

boolean 
mh_oauth_get_service_info(const char *svc_name, mh_oauth_service_info *svcinfo,
			  char *errbuf, size_t errbuflen);

char *mh_oauth_get_svc_name(mh_oauth_ctx *ctx);
void mh_oauth_set_cred_fn(mh_oauth_ctx *ctx, char *filename);
