/*
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
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>
#include <thirdparty/jsmn/jsmn.h>

#include <h/oauth.h>
#include <h/utils.h>

#define JSON_TYPE "application/json"

/* We pretend access tokens expire 30 seconds earlier than they actually do to
 * allow for separate processes to use and refresh access tokens.  The process
 * that uses the access token (post) has an error if the token is expired; the
 * process that refreshes the access token (send) must have already refreshed if
 * the expiration is close.
 *
 * 30s is arbitrary, and hopefully is enough to allow for clock skew.
 * Currently only Gmail supports XOAUTH2, and seems to always use a token
 * life-time of 3600s, but that is not guaranteed.  It is possible for Gmail to
 * issue an access token with a life-time so short that even after send
 * refreshes it, it's already expired when post tries to use it, but that seems
 * unlikely. */
#define EXPIRY_FUDGE 60

/* maximum size for HTTP response bodies
 * (not counting header and not null-terminated) */
#define RESPONSE_BODY_MAX 8192

/* Maxium size for URLs and URI-encoded query strings, null-terminated.
 *
 * Actual maximum we need is based on the size of tokens (limited by
 * RESPONSE_BODY_MAX), code user copies from a web page (arbitrarily large), and
 * various service parameters (all arbitrarily large).  In practice, all these
 * are just tens of bytes.  It's not hard to change this to realloc as needed,
 * but we should still have some limit, so why not this one?
 */
#define URL_MAX 8192

struct service_info {
    /* Name of service, so we can search static SERVICES (below) and for
     * determining default credential file name. */
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

static const struct service_info SERVICES[] = {
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

struct mh_oauth_cred {
    mh_oauth_ctx *ctx;

    /* opaque access token ([1] 1.4) in null-terminated string */
    char *access_token;
    /* opaque refresh token ([1] 1.5) in null-terminated string */
    char *refresh_token;

    /* time at which the access token expires, or 0 if unknown */
    time_t expires_at;

    /* Ignoring token_type ([1] 7.1) because
     * https://developers.google.com/accounts/docs/OAuth2InstalledApp says
     * "Currently, this field always has the value Bearer". */

    /* only filled while loading cred files, otherwise NULL */
    char *user;
};

struct mh_oauth_ctx {
    struct service_info svc;
    CURL *curl;
    FILE *log;

    char buf[URL_MAX];

    char *cred_fn;
    char *sasl_client_res;
    char *user_agent;

    mh_oauth_err_code err_code;

    /* If any detailed message about the error is available, this points to it.
     * May point to err_buf, or something else. */
    const char *err_details;

    /* Pointer to buffer mh_oauth_err_get_string allocates. */
    char *err_formatted;

    /* Ask libcurl to store errors here. */
    char err_buf[CURL_ERROR_SIZE];
};

struct curl_ctx {
    /* inputs */

    CURL *curl;
    /* NULL or a file handle to have curl log diagnostics to */
    FILE *log;

    /* outputs */

    /* Whether the response was too big; if so, the rest of the output fields
     * are undefined. */
    boolean too_big;

    /* HTTP response code */
    long res_code;

    /* NULL or null-terminated value of Content-Type response header field */
    const char *content_type;

    /* number of bytes in the response body */
    size_t res_len;

    /* response body; NOT null-terminated */
    char res_body[RESPONSE_BODY_MAX];
};

static boolean get_json_strings(const char *, size_t, FILE *, ...);
static boolean make_query_url(char *, size_t, CURL *, const char *, ...);
static boolean post(struct curl_ctx *, const char *, const char *);

char *
mh_oauth_do_xoauth(const char *user, const char *svc, FILE *log)
{
    mh_oauth_ctx *ctx;
    mh_oauth_cred *cred;
    char *fn;
    int failed_to_lock = 0;
    FILE *fp;
    size_t client_res_len;
    char *client_res;
    char *client_res_b64;

    if (!mh_oauth_new (&ctx, svc)) adios(NULL, mh_oauth_get_err_string(ctx));

    if (log != NULL) mh_oauth_log_to(stderr, ctx);

    fn = getcpy(mh_oauth_cred_fn(ctx));
    fp = lkfopendata(fn, "r+", &failed_to_lock);
    if (fp == NULL) {
        if (errno == ENOENT) {
            adios(NULL, "no credentials -- run mhlogin -saslmech xoauth2 -authservice %s", svc);
        }
        adios(fn, "failed to open");
    }
    if (failed_to_lock) {
        adios(fn, "failed to lock");
    }

    if ((cred = mh_oauth_cred_load(fp, ctx, user)) == NULL) {
        adios(NULL, mh_oauth_get_err_string(ctx));
    }

    if (!mh_oauth_access_token_valid(time(NULL), cred)) {
        if (!mh_oauth_refresh(cred)) {
            if (mh_oauth_get_err_code(ctx) == MH_OAUTH_NO_REFRESH) {
                adios(NULL, "no valid credentials -- run mhlogin -saslmech xoauth2 -authservice %s", svc);
            }
            if (mh_oauth_get_err_code(ctx) == MH_OAUTH_BAD_GRANT) {
                adios(NULL, "credentials rejected -- run mhlogin -saslmech xoauth2 -authservice %s", svc);
            }
            advise(NULL, "error refreshing OAuth2 token");
            adios(NULL, mh_oauth_get_err_string(ctx));
        }

        fseek(fp, 0, SEEK_SET);
        if (!mh_oauth_cred_save(fp, cred, user)) {
            adios(NULL, mh_oauth_get_err_string(ctx));
        }
    }

    if (lkfclosedata(fp, fn) < 0) {
        adios(fn, "failed to close");
    }
    free(fn);

    /* XXX writeBase64raw modifies the source buffer!  make a copy */
    client_res = getcpy(mh_oauth_sasl_client_response(&client_res_len, user,
                                                      cred));
    mh_oauth_cred_free(cred);
    mh_oauth_free(ctx);
    client_res_b64 = mh_xmalloc(((((client_res_len) + 2) / 3 ) * 4) + 1);
    if (writeBase64raw((unsigned char *)client_res, client_res_len,
                       (unsigned char *)client_res_b64) != OK) {
        adios(NULL, "base64 encoding of XOAUTH2 client response failed");
    }
    free(client_res);

    return client_res_b64;
}

static boolean
is_json(const char *content_type)
{
    return content_type != NULL
        && strncasecmp(content_type, JSON_TYPE, sizeof JSON_TYPE - 1) == 0;
}

static void
set_err_details(mh_oauth_ctx *ctx, mh_oauth_err_code code, const char *details)
{
    ctx->err_code = code;
    ctx->err_details = details;
}

static void
set_err(mh_oauth_ctx *ctx, mh_oauth_err_code code)
{
    set_err_details(ctx, code, NULL);
}

static void
set_err_http(mh_oauth_ctx *ctx, const struct curl_ctx *curl_ctx)
{
    char *error = NULL;
    mh_oauth_err_code code;
    /* 5.2. Error Response says error response should use status code 400 and
     * application/json body.  If Content-Type matches, try to parse the body
     * regardless of the status code. */
    if (curl_ctx->res_len > 0
        && is_json(curl_ctx->content_type)
        && get_json_strings(curl_ctx->res_body, curl_ctx->res_len, ctx->log,
                            "error", &error, (void *)NULL)
        && error != NULL) {
        if (strcmp(error, "invalid_grant") == 0) {
            code = MH_OAUTH_BAD_GRANT;
        } else {
            /* All other errors indicate a bug, not anything the user did. */
            code = MH_OAUTH_REQUEST_BAD;
        }
    } else {
        code = MH_OAUTH_RESPONSE_BAD;
    }
    set_err(ctx, code);
    free(error);
}

/* Copy service info so we don't have to free it only sometimes. */
static void
copy_svc(struct service_info *to, const struct service_info *from)
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
static char *
node_name_for_svc(const char *base_name, const char *svc)
{
    char *result = mh_xmalloc(sizeof "oauth-" - 1
                              + strlen(svc)
                              + 1            /* '-' */
                              + strlen(base_name)
                              + 1            /* '\0' */);
    sprintf(result, "oauth-%s-%s", svc, base_name);
    /* TODO: s/_/-/g ? */
    return result;
}

/* Update one service_info field if overridden in profile. */
static void
update_svc_field(char **field, const char *base_name, const char *svc)
{
    char *name = node_name_for_svc(base_name, svc);
    const char *value = context_find(name);
    if (value != NULL) {
        free(*field);
        *field = getcpy(value);
    }
    free(name);
}

/* Update all service_info fields that are overridden in profile. */
static boolean
update_svc(struct service_info *svc, const char *svc_name, mh_oauth_ctx *ctx)
{
#define update(name)                                                    \
    update_svc_field(&svc->name, #name, svc_name);                       \
    if (svc->name == NULL) {                                             \
        set_err_details(ctx, MH_OAUTH_BAD_PROFILE, #name " is missing"); \
        return FALSE;                                                    \
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

    return TRUE;
}

static char *
make_user_agent()
{
    const char *curl = curl_version_info(CURLVERSION_NOW)->version;
    char *s = mh_xmalloc(strlen(user_agent)
                         + 1
                         + sizeof "libcurl"
                         + 1
                         + strlen(curl)
                         + 1);
    sprintf(s, "%s libcurl/%s", user_agent, curl);
    return s;
}

boolean
mh_oauth_new(mh_oauth_ctx **result, const char *svc_name)
{
    mh_oauth_ctx *ctx = *result = mh_xmalloc(sizeof *ctx);
    size_t i;

    ctx->curl = NULL;

    ctx->log = NULL;
    ctx->cred_fn = ctx->sasl_client_res = ctx->err_formatted = NULL;

    ctx->svc.name = ctx->svc.display_name = NULL;
    ctx->svc.scope = ctx->svc.client_id = NULL;
    ctx->svc.client_secret = ctx->svc.auth_endpoint = NULL;
    ctx->svc.token_endpoint = ctx->svc.redirect_uri = NULL;

    for (i = 0; i < sizeof SERVICES / sizeof SERVICES[0]; i++) {
        if (strcmp(SERVICES[i].name, svc_name) == 0) {
            copy_svc(&ctx->svc, &SERVICES[i]);
            break;
        }
    }

    if (!update_svc(&ctx->svc, svc_name, ctx)) {
        return FALSE;
    }

    ctx->curl = curl_easy_init();
    if (ctx->curl == NULL) {
        set_err(ctx, MH_OAUTH_CURL_INIT);
        return FALSE;
    }
    curl_easy_setopt(ctx->curl, CURLOPT_ERRORBUFFER, ctx->err_buf);

    ctx->user_agent = make_user_agent();

    if (curl_easy_setopt(ctx->curl, CURLOPT_USERAGENT,
                         ctx->user_agent) != CURLE_OK) {
        set_err_details(ctx, MH_OAUTH_CURL_INIT, ctx->err_buf);
        return FALSE;
    }

    return TRUE;
}

void
mh_oauth_free(mh_oauth_ctx *ctx)
{
    free(ctx->svc.name);
    free(ctx->svc.scope);
    free(ctx->svc.client_id);
    free(ctx->svc.client_secret);
    free(ctx->svc.auth_endpoint);
    free(ctx->svc.token_endpoint);
    free(ctx->svc.redirect_uri);
    free(ctx->cred_fn);
    free(ctx->sasl_client_res);
    free(ctx->err_formatted);
    free(ctx->user_agent);

    if (ctx->curl != NULL) {
        curl_easy_cleanup(ctx->curl);
    }
    free(ctx);
}

const char *
mh_oauth_svc_display_name(const mh_oauth_ctx *ctx)
{
    return ctx->svc.display_name;
}

void
mh_oauth_log_to(FILE *log, mh_oauth_ctx *ctx)
{
    ctx->log = log;
}

mh_oauth_err_code
mh_oauth_get_err_code(const mh_oauth_ctx *ctx)
{
    return ctx->err_code;
}

const char *
mh_oauth_get_err_string(mh_oauth_ctx *ctx)
{
    char *result;
    const char *base;

    free(ctx->err_formatted);

    switch (ctx->err_code) {
    case MH_OAUTH_BAD_PROFILE:
        base = "incomplete OAuth2 service definition";
        break;
    case MH_OAUTH_CURL_INIT:
        base = "error initializing libcurl";
        break;
    case MH_OAUTH_REQUEST_INIT:
        base = "local error initializing HTTP request";
        break;
    case MH_OAUTH_POST:
        base = "error making HTTP request to OAuth2 authorization endpoint";
        break;
    case MH_OAUTH_RESPONSE_TOO_BIG:
        base = "refusing to process response body larger than 8192 bytes";
        break;
    case MH_OAUTH_RESPONSE_BAD:
        base = "invalid response";
        break;
    case MH_OAUTH_BAD_GRANT:
        base = "bad grant (authorization code or refresh token)";
        break;
    case MH_OAUTH_REQUEST_BAD:
        base = "bad OAuth request; re-run with -snoop and send REDACTED output"
            " to nmh-workers";
        break;
    case MH_OAUTH_NO_REFRESH:
        base = "no refresh token";
        break;
    case MH_OAUTH_CRED_USER_NOT_FOUND:
        base = "user not found in cred file";
        break;
    case MH_OAUTH_CRED_FILE:
        base = "error loading cred file";
        break;
    default:
        base = "unknown error";
    }
    if (ctx->err_details == NULL) {
        return ctx->err_formatted = getcpy(base);
    }
    /* length of the two strings plus ": " and '\0' */
    result = mh_xmalloc(strlen(base) + strlen(ctx->err_details) + 3);
    sprintf(result, "%s: %s", base, ctx->err_details);
    return ctx->err_formatted = result;
}

const char *
mh_oauth_get_authorize_url(mh_oauth_ctx *ctx)
{
    /* [1] 4.1.1 Authorization Request */
    if (!make_query_url(ctx->buf, sizeof ctx->buf, ctx->curl,
                        ctx->svc.auth_endpoint,
                        "response_type", "code",
                        "client_id", ctx->svc.client_id,
                        "redirect_uri", ctx->svc.redirect_uri,
                        "scope", ctx->svc.scope,
                        (void *)NULL)) {
        set_err(ctx, MH_OAUTH_REQUEST_INIT);
        return NULL;
    }
    return ctx->buf;
}

static boolean
cred_from_response(mh_oauth_cred *cred, const char *content_type,
                   const char *input, size_t input_len)
{
    boolean result = FALSE;
    char *access_token, *expires_in, *refresh_token;
    const mh_oauth_ctx *ctx = cred->ctx;

    if (!is_json(content_type)) {
        return FALSE;
    }

    access_token = expires_in = refresh_token = NULL;
    if (!get_json_strings(input, input_len, ctx->log,
                          "access_token", &access_token,
                          "expires_in", &expires_in,
                          "refresh_token", &refresh_token,
                          (void *)NULL)) {
        goto out;
    }

    if (access_token == NULL) {
        /* Response is invalid, but if it has a refresh token, we can try. */
        if (refresh_token == NULL) {
            goto out;
        }
    }

    result = TRUE;

    free(cred->access_token);
    cred->access_token = access_token;
    access_token = NULL;

    cred->expires_at = 0;
    if (expires_in != NULL) {
        long e;
        errno = 0;
        e = strtol(expires_in, NULL, 10);
        if (errno == 0) {
            if (e > 0) {
                cred->expires_at = time(NULL) + e;
            }
        } else if (ctx->log != NULL) {
            fprintf(ctx->log, "* invalid expiration: %s\n", expires_in);
        }
    }

    /* [1] 6 Refreshing an Access Token says a new refresh token may be issued
     * in refresh responses. */
    if (refresh_token != NULL) {
        free(cred->refresh_token);
        cred->refresh_token = refresh_token;
        refresh_token = NULL;
    }

  out:
    free(refresh_token);
    free(expires_in);
    free(access_token);
    return result;
}

static boolean
do_access_request(mh_oauth_cred *cred, const char *req_body)
{
    mh_oauth_ctx *ctx = cred->ctx;
    struct curl_ctx curl_ctx;

    curl_ctx.curl = ctx->curl;
    curl_ctx.log = ctx->log;
    if (!post(&curl_ctx, ctx->svc.token_endpoint, req_body)) {
        if (curl_ctx.too_big) {
            set_err(ctx, MH_OAUTH_RESPONSE_TOO_BIG);
        } else {
            set_err_details(ctx, MH_OAUTH_POST, ctx->err_buf);
        }
        return FALSE;
    }

    if (curl_ctx.res_code != 200) {
        set_err_http(ctx, &curl_ctx);
        return FALSE;
    }

    if (!cred_from_response(cred, curl_ctx.content_type, curl_ctx.res_body,
                            curl_ctx.res_len)) {
        set_err(ctx, MH_OAUTH_RESPONSE_BAD);
        return FALSE;
    }

    return TRUE;
}

mh_oauth_cred *
mh_oauth_authorize(const char *code, mh_oauth_ctx *ctx)
{
    mh_oauth_cred *result;

    if (!make_query_url(ctx->buf, sizeof ctx->buf, ctx->curl, NULL,
                        "code", code,
                        "grant_type", "authorization_code",
                        "redirect_uri", ctx->svc.redirect_uri,
                        "client_id", ctx->svc.client_id,
                        "client_secret", ctx->svc.client_secret,
                        (void *)NULL)) {
        set_err(ctx, MH_OAUTH_REQUEST_INIT);
        return NULL;
    }

    result = mh_xmalloc(sizeof *result);
    result->ctx = ctx;
    result->access_token = result->refresh_token = NULL;

    if (!do_access_request(result, ctx->buf)) {
        free(result);
        return NULL;
    }

    return result;
}

boolean
mh_oauth_refresh(mh_oauth_cred *cred)
{
    boolean result;
    mh_oauth_ctx *ctx = cred->ctx;

    if (cred->refresh_token == NULL) {
        set_err(ctx, MH_OAUTH_NO_REFRESH);
        return FALSE;
    }

    if (!make_query_url(ctx->buf, sizeof ctx->buf, ctx->curl, NULL,
                        "grant_type", "refresh_token",
                        "refresh_token", cred->refresh_token,
                        "client_id", ctx->svc.client_id,
                        "client_secret", ctx->svc.client_secret,
                        (void *)NULL)) {
        set_err(ctx, MH_OAUTH_REQUEST_INIT);
        return FALSE;
    }

    result = do_access_request(cred, ctx->buf);

    if (result && cred->access_token == NULL) {
        set_err_details(ctx, MH_OAUTH_RESPONSE_BAD, "no access token");
        return FALSE;
    }

    return result;
}

boolean
mh_oauth_access_token_valid(time_t t, const mh_oauth_cred *cred)
{
    return cred->access_token != NULL && t + EXPIRY_FUDGE < cred->expires_at;
}

void
mh_oauth_cred_free(mh_oauth_cred *cred)
{
    free(cred->refresh_token);
    free(cred->access_token);
    free(cred);
}

const char *
mh_oauth_cred_fn(mh_oauth_ctx *ctx)
{
    char *result, *result_if_allocated;
    const char *svc = ctx->svc.name;

    char *component = node_name_for_svc("credential-file", svc);
    result = context_find(component);
    free(component);

    if (result == NULL) {
        result = mh_xmalloc(sizeof "oauth-" - 1
                            + strlen(svc)
                            + 1 /* '\0' */);
        sprintf(result, "oauth-%s", svc);
        result_if_allocated = result;
    } else {
        result_if_allocated = NULL;
    }

    if (result[0] != '/') {
        const char *tmp = m_maildir(result);
        free(result_if_allocated);
        result = getcpy(tmp);
    }

    free(ctx->cred_fn);
    return ctx->cred_fn = result;
}

/* for loading multi-user cred files */
struct user_creds {
    mh_oauth_cred *creds;

    /* number of allocated mh_oauth_cred structs above points to */
    size_t alloc;

    /* number that are actually filled in and used */
    size_t len;
};

/* If user has an entry in user_creds, return pointer to it.  Else allocate a
 * new struct in user_creds and return pointer to that. */
static mh_oauth_cred *
find_or_alloc_user_creds(struct user_creds user_creds[], const char *user)
{
    mh_oauth_cred *creds = user_creds->creds;
    size_t i;
    for (i = 0; i < user_creds->len; i++) {
        if (strcmp(creds[i].user, user) == 0) {
            return &creds[i];
        }
    }
    if (user_creds->alloc == user_creds->len) {
        user_creds->alloc *= 2;
        user_creds->creds = mh_xrealloc(user_creds->creds, user_creds->alloc);
    }
    creds = user_creds->creds+user_creds->len;
    user_creds->len++;
    creds->user = getcpy(user);
    creds->access_token = creds->refresh_token = NULL;
    creds->expires_at = 0;
    return creds;
}

static void
free_user_creds(struct user_creds *user_creds)
{
    mh_oauth_cred *cred;
    size_t i;
    cred = user_creds->creds;
    for (i = 0; i < user_creds->len; i++) {
        free(cred[i].user);
        free(cred[i].access_token);
        free(cred[i].refresh_token);
    }
    free(user_creds->creds);
    free(user_creds);
}

static boolean
load_creds(struct user_creds **result, FILE *fp, mh_oauth_ctx *ctx)
{
    boolean success = FALSE;
    char name[NAMESZ], value_buf[BUFSIZ];
    int state;
    m_getfld_state_t getfld_ctx = 0;

    struct user_creds *user_creds = mh_xmalloc(sizeof *user_creds);
    user_creds->alloc = 4;
    user_creds->len = 0;
    user_creds->creds = mh_xmalloc(user_creds->alloc * sizeof *user_creds->creds);

    for (;;) {
	int size = sizeof value_buf;
	switch (state = m_getfld(&getfld_ctx, name, value_buf, &size, fp)) {
        case FLD:
        case FLDPLUS: {
            char **save, *expire;
            time_t *expires_at = NULL;
            if (strncmp(name, "access-", 7) == 0) {
                const char *user = name + 7;
                mh_oauth_cred *creds = find_or_alloc_user_creds(user_creds,
                                                                user);
                save = &creds->access_token;
            } else if (strncmp(name, "refresh-", 8) == 0) {
                const char *user = name + 8;
                mh_oauth_cred *creds = find_or_alloc_user_creds(user_creds,
                                                                user);
                save = &creds->refresh_token;
            } else if (strncmp(name, "expire-", 7) == 0) {
                const char *user = name + 7;
                mh_oauth_cred *creds = find_or_alloc_user_creds(user_creds,
                                                                user);
                expires_at = &creds->expires_at;
                save = &expire;
            } else {
                set_err_details(ctx, MH_OAUTH_CRED_FILE, "unexpected field");
                break;
            }

            if (state == FLD) {
                *save = trimcpy(value_buf);
            } else {
                char *tmp = getcpy(value_buf);
                while (state == FLDPLUS) {
                    size = sizeof value_buf;
                    state = m_getfld(&getfld_ctx, name, value_buf, &size, fp);
                    tmp = add(value_buf, tmp);
                }
                *save = trimcpy(tmp);
                free(tmp);
            }
            if (expires_at != NULL) {
                errno = 0;
                *expires_at = strtol(expire, NULL, 10);
                free(expire);
                if (errno != 0) {
                    set_err_details(ctx, MH_OAUTH_CRED_FILE,
                                    "invalid expiration time");
                    break;
                }
                expires_at = NULL;
            }
            continue;
        }

        case BODY:
        case FILEEOF:
            success = TRUE;
            break;

        default:
            /* Not adding details for LENERR/FMTERR because m_getfld already
             * wrote advise message to stderr. */
            set_err(ctx, MH_OAUTH_CRED_FILE);
            break;
	}
	break;
    }
    m_getfld_state_destroy(&getfld_ctx);

    if (success) {
        *result = user_creds;
    } else {
        free_user_creds(user_creds);
    }

    return success;
}

static boolean
save_user(FILE *fp, const char *user, const char *access, const char *refresh,
          long expires_at)
{
    if (access != NULL) {
        if (fprintf(fp, "access-%s: %s\n", user, access) < 0) return FALSE;
    }
    if (refresh != NULL) {
        if (fprintf(fp, "refresh-%s: %s\n", user, refresh) < 0) return FALSE;
    }
    if (expires_at > 0) {
        if (fprintf(fp, "expire-%s: %ld\n", user, (long)expires_at) < 0) {
            return FALSE;
        }
    }
    return TRUE;
}

boolean
mh_oauth_cred_save(FILE *fp, mh_oauth_cred *cred, const char *user)
{
    struct user_creds *user_creds;
    int fd = fileno(fp);
    size_t i;

    /* Load existing creds if any. */
    if (!load_creds(&user_creds, fp, cred->ctx)) {
        return FALSE;
    }

    if (fchmod(fd, S_IRUSR | S_IWUSR) < 0) goto err;
    if (ftruncate(fd, 0) < 0) goto err;
    if (fseek(fp, 0, SEEK_SET) < 0) goto err;

    /* Write all creds except for this user. */
    for (i = 0; i < user_creds->len; i++) {
        mh_oauth_cred *c = &user_creds->creds[i];
        if (strcmp(c->user, user) == 0) continue;
        if (!save_user(fp, c->user, c->access_token, c->refresh_token,
                       c->expires_at)) {
            goto err;
        }
    }

    /* Write updated creds for this user. */
    if (!save_user(fp, user, cred->access_token, cred->refresh_token,
                   cred->expires_at)) {
        goto err;
    }

    free_user_creds(user_creds);

    return TRUE;

  err:
    free_user_creds(user_creds);
    set_err(cred->ctx, MH_OAUTH_CRED_FILE);
    return FALSE;
}

mh_oauth_cred *
mh_oauth_cred_load(FILE *fp, mh_oauth_ctx *ctx, const char *user)
{
    mh_oauth_cred *creds, *result = NULL;
    struct user_creds *user_creds;
    size_t i;

    if (!load_creds(&user_creds, fp, ctx)) {
        return NULL;
    }

    /* Search user_creds for this user.  If we don't find it, return NULL.
     * If we do, free fields of all structs except this one, moving this one to
     * the first struct if necessary.  When we return it, it just looks like one
     * struct to the caller, and the whole array is freed later. */
    creds = user_creds->creds;
    for (i = 0; i < user_creds->len; i++) {
        if (strcmp(creds[i].user, user) == 0) {
            result = creds;
            if (i > 0) {
                result->access_token = creds[i].access_token;
                result->refresh_token = creds[i].refresh_token;
                result->expires_at = creds[i].expires_at;
            }
        } else {
            free(creds[i].access_token);
            free(creds[i].refresh_token);
        }
        free(creds[i].user);
    }

    if (result == NULL) {
        set_err_details(ctx, MH_OAUTH_CRED_USER_NOT_FOUND, user);
        return NULL;
    }

    result->ctx = ctx;
    result->user = NULL;

    return result;
}

const char *
mh_oauth_sasl_client_response(size_t *res_len,
                              const char *user, const mh_oauth_cred *cred)
{
    size_t len = sizeof "user=" - 1
        + strlen(user)
        + sizeof "\1auth=Bearer " - 1
        + strlen(cred->access_token)
        + sizeof "\1\1" - 1;
    free(cred->ctx->sasl_client_res);
    cred->ctx->sasl_client_res = mh_xmalloc(len + 1);
    *res_len = len;
    sprintf(cred->ctx->sasl_client_res, "user=%s\1auth=Bearer %s\1\1",
            user, cred->access_token);
    return cred->ctx->sasl_client_res;
}

/*******************************************************************************
 * building URLs and making HTTP requests with libcurl
 */

/*
 * Build null-terminated URL in the array pointed to by s.  If the URL doesn't
 * fit within size (including the terminating null byte), return FALSE without *
 * building the entire URL.  Some of URL may already have been written into the
 * result array in that case.
 */
static boolean
make_query_url(char *s, size_t size, CURL *curl, const char *base_url, ...)
{
    boolean result = FALSE;
    size_t len;
    char *prefix;
    va_list ap;
    const char *name;

    if (base_url == NULL) {
        len = 0;
        prefix = "";
    } else {
        len = sprintf(s, "%s", base_url);
        prefix = "?";
    }

    va_start(ap, base_url);
    for (name = va_arg(ap, char *); name != NULL; name = va_arg(ap, char *)) {
        char *name_esc = curl_easy_escape(curl, name, 0);
        char *val_esc = curl_easy_escape(curl, va_arg(ap, char *), 0);
        /* prefix + name_esc + '=' + val_esc + '\0' must fit within size */
        size_t new_len = len
          + strlen(prefix)
          + strlen(name_esc)
          + 1 /* '=' */
          + strlen(val_esc);
        if (new_len + 1 > size) {
            free(name_esc);
            free(val_esc);
            goto out;
        }
        sprintf(s + len, "%s%s=%s", prefix, name_esc, val_esc);
        free(name_esc);
        free(val_esc);
        len = new_len;
        prefix = "&";
    }

    result = TRUE;

  out:
    va_end(ap);
    return result;
}

static int
debug_callback(const CURL *handle, curl_infotype type, const char *data,
               size_t size, void *userptr)
{
    FILE *fp = userptr;
    NMH_UNUSED(handle);

    switch (type) {
    case CURLINFO_HEADER_IN:
    case CURLINFO_DATA_IN:
        fputs("< ", fp);
        break;
    case CURLINFO_HEADER_OUT:
    case CURLINFO_DATA_OUT:
        fputs("> ", fp);
        break;
    default:
        return 0;
    }
    fwrite(data, 1, size, fp);
    if (data[size - 1] != '\n') {
        fputs("\n", fp);
    }
    fflush(fp);
    return 0;
}

static size_t
write_callback(const char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct curl_ctx *ctx = userdata;
    size_t new_len;

    if (ctx->too_big) {
        return 0;
    }

    size *= nmemb;
    new_len = ctx->res_len + size;
    if (new_len > sizeof ctx->res_body) {
      ctx->too_big = TRUE;
      return 0;
    }

    memcpy(ctx->res_body + ctx->res_len, ptr, size);
    ctx->res_len = new_len;

    return size;
}

static boolean
post(struct curl_ctx *ctx, const char *url, const char *req_body)
{
    CURL *curl = ctx->curl;
    CURLcode status;

    ctx->too_big = FALSE;
    ctx->res_len = 0;

    if (ctx->log != NULL) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, (long)1);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, ctx->log);
    }

    if ((status = curl_easy_setopt(curl, CURLOPT_URL, url)) != CURLE_OK) {
        return FALSE;
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);

    status = curl_easy_perform(curl);
    /* first check for error from callback */
    if (ctx->too_big) {
        return FALSE;
    }
    /* now from curl */
    if (status != CURLE_OK) {
        return FALSE;
    }

    if ((status = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                    &ctx->res_code)) != CURLE_OK
        || (status = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE,
                                       &ctx->content_type)) != CURLE_OK) {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
 * JSON processing
 */

/* We need 2 for each key/value pair plus 1 for the enclosing object, which
 * means we only need 9 for Gmail.  Clients must not fail if the server returns
 * more, though, e.g. for protocol extensions. */
#define JSMN_TOKENS 16

/*
 * Parse JSON, store pointer to array of jsmntok_t in tokens.
 *
 * Returns whether parsing is successful.
 *
 * Even in that case, tokens has been allocated and must be freed.
 */
static boolean
parse_json(jsmntok_t **tokens, size_t *tokens_len,
           const char *input, size_t input_len, FILE *log)
{
    jsmn_parser p;
    jsmnerr_t r;

    *tokens_len = JSMN_TOKENS;
    *tokens = mh_xmalloc(*tokens_len * sizeof **tokens);

    jsmn_init(&p);
    while ((r = jsmn_parse(&p, input, input_len,
                           *tokens, *tokens_len)) == JSMN_ERROR_NOMEM) {
        *tokens_len = 2 * *tokens_len;
        if (log != NULL) {
            fprintf(log, "* need more jsmntok_t! allocating %ld\n",
                    (long)*tokens_len);
        }
        /* Don't need to limit how much we allocate; we already limited the size
           of the response body. */
        *tokens = mh_xrealloc(*tokens, *tokens_len * sizeof **tokens);
    }
    if (r == 0) {
        return FALSE;
    }

    return TRUE;
}

/*
 * Search input and tokens for the value identified by null-terminated name.
 *
 * If found, allocate a null-terminated copy of the value and store the address
 * in val.  val is left untouched if not found.
 */
static void
get_json_string(char **val, const char *input, const jsmntok_t *tokens,
                const char *name)
{
    /* number of top-level tokens (not counting object/list children) */
    int token_count = tokens[0].size * 2;
    /* number of tokens to skip when we encounter objects and lists */
    /* We only look for top-level strings. */
    int skip_tokens = 0;
    /* whether the current token represents a field name */
    /* The next token will be the value. */
    boolean is_key = TRUE;

    int i;
    for (i = 1; i <= token_count; i++) {
        const char *key;
        int key_len;
        if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
            /* We're not interested in any array or object children; skip. */
            int children = tokens[i].size;
            if (tokens[i].type == JSMN_OBJECT) {
                /* Object size counts key/value pairs, skip both. */
                children *= 2;
            }
            /* Add children to token_count. */
            token_count += children;
            if (skip_tokens == 0) {
                /* This token not already skipped; skip it. */
                /* Would already be skipped if child of object or list. */
                skip_tokens++;
            }
            /* Skip this token's children. */
            skip_tokens += children;
        }
        if (skip_tokens > 0) {
            skip_tokens--;
            /* When we finish with the object or list, we'll have a key. */
            is_key = TRUE;
            continue;
        }
        if (is_key) {
            is_key = FALSE;
            continue;
        }
        key = input + tokens[i - 1].start;
        key_len = tokens[i - 1].end - tokens[i - 1].start;
        if (strncmp(key, name, key_len) == 0) {
            int val_len = tokens[i].end - tokens[i].start;
            *val = mh_xmalloc(val_len + 1);
            memcpy(*val, input + tokens[i].start, val_len);
            (*val)[val_len] = '\0';
            return;
        }
        is_key = TRUE;
    }
}

/*
 * Parse input as JSON, extracting specified string values.
 *
 * Variadic arguments are pairs of null-terminated strings indicating the value
 * to extract from the JSON and addresses into which pointers to null-terminated
 * copies of the values are written.  These must be followed by one NULL pointer
 * to indicate the end of pairs.
 *
 * The extracted strings are copies which caller must free.  If any name is not
 * found, the address to store the value is not touched.
 *
 * Returns non-zero if parsing is successful.
 *
 * When parsing failed, no strings have been copied.
 *
 * log may be used for debug-logging if not NULL.
 */
static boolean
get_json_strings(const char *input, size_t input_len, FILE *log, ...)
{
    boolean result = FALSE;
    jsmntok_t *tokens;
    size_t tokens_len;
    va_list ap;
    const char *name;

    if (!parse_json(&tokens, &tokens_len, input, input_len, log)) {
        goto out;
    }

    if (tokens->type != JSMN_OBJECT || tokens->size == 0) {
        goto out;
    }

    result = TRUE;

    va_start(ap, log);
    for (name = va_arg(ap, char *); name != NULL; name = va_arg(ap, char *)) {
        get_json_string(va_arg(ap, char **), input, tokens, name);
    }

  out:
    va_end(ap);
    free(tokens);
    return result;
}

#endif
