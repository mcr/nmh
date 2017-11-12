/* oauth.c -- OAuth 2.0 implementation for XOAUTH2 in SMTP and POP3.
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "concat.h"
#include "trimcpy.h"
#include "getcpy.h"
#include "error.h"

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
#include "thirdparty/jsmn/jsmn.h"

#include "h/oauth.h"
#include "h/utils.h"
#include "lock_file.h"

#define JSON_TYPE "application/json"

/* We pretend access tokens expire 60 seconds earlier than they actually do to
 * allow for separate processes to use and refresh access tokens.  The process
 * that uses the access token (post) has an error if the token is expired; the
 * process that refreshes the access token (send) must have already refreshed if
 * the expiration is close.
 *
 * 60s is arbitrary, and hopefully is enough to allow for clock skew.
 * Currently only Gmail supports XOAUTH2, and seems to always use a token
 * life-time of 3600s, but that is not guaranteed.  It is possible for Gmail to
 * issue an access token with a life-time so short that even after send
 * refreshes it, it's already expired when post tries to use it, but that seems
 * unlikely. */
#define EXPIRY_FUDGE 60

/* maximum size for HTTP response bodies
 * (not counting header and not null-terminated) */
#define RESPONSE_BODY_MAX 8192

/* Maximum size for URLs and URI-encoded query strings, null-terminated.
 *
 * Actual maximum we need is based on the size of tokens (limited by
 * RESPONSE_BODY_MAX), code user copies from a web page (arbitrarily large), and
 * various service parameters (all arbitrarily large).  In practice, all these
 * are just tens of bytes.  It's not hard to change this to realloc as needed,
 * but we should still have some limit, so why not this one?
 */
#define URL_MAX 8192

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
    struct mh_oauth_service_info svc;
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
    bool too_big;

    /* HTTP response code */
    long res_code;

    /* NULL or null-terminated value of Content-Type response header field */
    const char *content_type;

    /* number of bytes in the response body */
    size_t res_len;

    /* response body; NOT null-terminated */
    char res_body[RESPONSE_BODY_MAX];
};

static bool get_json_strings(const char *, size_t, FILE *, ...) ENDNULL;
static bool make_query_url(char *, size_t, CURL *, const char *, ...) ENDNULL;
static bool post(struct curl_ctx *, const char *, const char *);

int
mh_oauth_do_xoauth(const char *user, const char *svc, unsigned char **oauth_res,
		   size_t *oauth_res_len, FILE *log)
{
    mh_oauth_ctx *ctx;
    mh_oauth_cred *cred;
    char *fn;
    int failed_to_lock = 0;
    FILE *fp;
    char *client_res;

    if (!mh_oauth_new (&ctx, svc))
        die("%s", mh_oauth_get_err_string(ctx));

    if (log != NULL) mh_oauth_log_to(stderr, ctx);

    fn = mh_oauth_cred_fn(svc);
    fp = lkfopendata(fn, "r+", &failed_to_lock);
    if (fp == NULL) {
        if (errno == ENOENT) {
            die("no credentials -- run mhlogin -saslmech xoauth2 -authservice %s", svc);
        }
        adios(fn, "failed to open");
    }
    if (failed_to_lock) {
        adios(fn, "failed to lock");
    }

    if ((cred = mh_oauth_cred_load(fp, ctx, user)) == NULL) {
        die("%s", mh_oauth_get_err_string(ctx));
    }

    if (!mh_oauth_access_token_valid(time(NULL), cred)) {
        if (!mh_oauth_refresh(cred)) {
            if (mh_oauth_get_err_code(ctx) == MH_OAUTH_NO_REFRESH) {
                die("no valid credentials -- run mhlogin -saslmech xoauth2 -authservice %s", svc);
            }
            if (mh_oauth_get_err_code(ctx) == MH_OAUTH_BAD_GRANT) {
                die("credentials rejected -- run mhlogin -saslmech xoauth2 -authservice %s", svc);
            }
            inform("error refreshing OAuth2 token");
            die("%s", mh_oauth_get_err_string(ctx));
        }

        fseek(fp, 0, SEEK_SET);
        if (!mh_oauth_cred_save(fp, cred, user)) {
            die("%s", mh_oauth_get_err_string(ctx));
        }
    }

    if (lkfclosedata(fp, fn) < 0) {
        adios(fn, "failed to close");
    }
    free(fn);

    /* XXX writeBase64raw modifies the source buffer!  make a copy */
    client_res = mh_xstrdup(mh_oauth_sasl_client_response(oauth_res_len, user,
                                                      cred));
    mh_oauth_cred_free(cred);
    mh_oauth_free(ctx);

    *oauth_res = (unsigned char *) client_res;

    return OK;
}

static bool
is_json(const char *content_type)
{
    return content_type != NULL
        && strncasecmp(content_type, JSON_TYPE, LEN(JSON_TYPE)) == 0;
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
                            "error", &error, NULL)
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

static char *
make_user_agent(void)
{
    const char *curl = curl_version_info(CURLVERSION_NOW)->version;
    return concat(user_agent, " libcurl/", curl, NULL);
}

bool
mh_oauth_new(mh_oauth_ctx **result, const char *svc_name)
{
    mh_oauth_ctx *ctx;

    NEW(ctx);
    *result = ctx;
    ctx->curl = NULL;

    ctx->log = NULL;
    ctx->cred_fn = ctx->sasl_client_res = ctx->err_formatted = NULL;

    if (!mh_oauth_get_service_info(svc_name, &ctx->svc, ctx->err_buf,
				   sizeof(ctx->err_buf))) {
	set_err_details(ctx, MH_OAUTH_BAD_PROFILE, ctx->err_buf);
        return false;
    }

    ctx->curl = curl_easy_init();
    if (ctx->curl == NULL) {
        set_err(ctx, MH_OAUTH_CURL_INIT);
        return false;
    }
    curl_easy_setopt(ctx->curl, CURLOPT_ERRORBUFFER, ctx->err_buf);

    ctx->user_agent = make_user_agent();

    if (curl_easy_setopt(ctx->curl, CURLOPT_USERAGENT,
                         ctx->user_agent) != CURLE_OK) {
        set_err_details(ctx, MH_OAUTH_CURL_INIT, ctx->err_buf);
        return false;
    }

    return true;
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
        return ctx->err_formatted = mh_xstrdup(base);
    }

    ctx->err_formatted = concat(base, ": ", ctx->err_details, NULL);
    return ctx->err_formatted;
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
                        NULL)) {
        set_err(ctx, MH_OAUTH_REQUEST_INIT);
        return NULL;
    }
    return ctx->buf;
}

static bool
cred_from_response(mh_oauth_cred *cred, const char *content_type,
                   const char *input, size_t input_len)
{
    bool result = false;
    char *access_token, *expires_in, *refresh_token;
    const mh_oauth_ctx *ctx = cred->ctx;

    if (!is_json(content_type)) {
        return false;
    }

    access_token = expires_in = refresh_token = NULL;
    if (!get_json_strings(input, input_len, ctx->log,
                          "access_token", &access_token,
                          "expires_in", &expires_in,
                          "refresh_token", &refresh_token,
                          NULL)) {
        goto out;
    }

    if (access_token == NULL) {
        /* Response is invalid, but if it has a refresh token, we can try. */
        if (refresh_token == NULL) {
            goto out;
        }
    }

    result = true;

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

static bool
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
        return false;
    }

    if (curl_ctx.res_code != 200) {
        set_err_http(ctx, &curl_ctx);
        return false;
    }

    if (!cred_from_response(cred, curl_ctx.content_type, curl_ctx.res_body,
                            curl_ctx.res_len)) {
        set_err(ctx, MH_OAUTH_RESPONSE_BAD);
        return false;
    }

    return true;
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
                        NULL)) {
        set_err(ctx, MH_OAUTH_REQUEST_INIT);
        return NULL;
    }

    NEW(result);
    result->ctx = ctx;
    result->access_token = result->refresh_token = NULL;

    if (!do_access_request(result, ctx->buf)) {
        free(result);
        return NULL;
    }

    return result;
}

bool
mh_oauth_refresh(mh_oauth_cred *cred)
{
    bool result;
    mh_oauth_ctx *ctx = cred->ctx;

    if (cred->refresh_token == NULL) {
        set_err(ctx, MH_OAUTH_NO_REFRESH);
        return false;
    }

    if (!make_query_url(ctx->buf, sizeof ctx->buf, ctx->curl, NULL,
                        "grant_type", "refresh_token",
                        "refresh_token", cred->refresh_token,
                        "client_id", ctx->svc.client_id,
                        "client_secret", ctx->svc.client_secret,
                        NULL)) {
        set_err(ctx, MH_OAUTH_REQUEST_INIT);
        return false;
    }

    result = do_access_request(cred, ctx->buf);

    if (result && cred->access_token == NULL) {
        set_err_details(ctx, MH_OAUTH_RESPONSE_BAD, "no access token");
        return false;
    }

    return result;
}

bool
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

static bool
load_creds(struct user_creds **result, FILE *fp, mh_oauth_ctx *ctx)
{
    bool success = false;
    char name[NAMESZ], value_buf[BUFSIZ];
    int state;
    m_getfld_state_t getfld_ctx;

    struct user_creds *user_creds;
    NEW(user_creds);
    user_creds->alloc = 4;
    user_creds->len = 0;
    user_creds->creds = mh_xmalloc(user_creds->alloc * sizeof *user_creds->creds);

    getfld_ctx = m_getfld_state_init(fp);
    for (;;) {
	int size = sizeof value_buf;
	switch (state = m_getfld2(&getfld_ctx, name, value_buf, &size)) {
        case FLD:
        case FLDPLUS: {
            char **save, *expire;
            time_t *expires_at = NULL;
            if (has_prefix(name, "access-")) {
                const char *user = name + 7;
                mh_oauth_cred *creds = find_or_alloc_user_creds(user_creds,
                                                                user);
                save = &creds->access_token;
            } else if (has_prefix(name, "refresh-")) {
                const char *user = name + 8;
                mh_oauth_cred *creds = find_or_alloc_user_creds(user_creds,
                                                                user);
                save = &creds->refresh_token;
            } else if (has_prefix(name, "expire-")) {
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
                    state = m_getfld2(&getfld_ctx, name, value_buf, &size);
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
            success = true;
            break;

        default:
            /* Not adding details for LENERR/FMTERR because m_getfld2 already
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

static bool
save_user(FILE *fp, const char *user, const char *access, const char *refresh,
          long expires_at)
{
    if (access != NULL) {
        if (fprintf(fp, "access-%s: %s\n", user, access) < 0) return false;
    }
    if (refresh != NULL) {
        if (fprintf(fp, "refresh-%s: %s\n", user, refresh) < 0) return false;
    }
    if (expires_at > 0) {
        if (fprintf(fp, "expire-%s: %ld\n", user, (long)expires_at) < 0) {
            return false;
        }
    }
    return true;
}

bool
mh_oauth_cred_save(FILE *fp, mh_oauth_cred *cred, const char *user)
{
    struct user_creds *user_creds;
    int fd = fileno(fp);
    size_t i;

    /* Load existing creds if any. */
    if (!load_creds(&user_creds, fp, cred->ctx)) {
        return false;
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

    return true;

  err:
    free_user_creds(user_creds);
    set_err(cred->ctx, MH_OAUTH_CRED_FILE);
    return false;
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

    /* No longer need user_creds.  result just uses its creds member. */
    free(user_creds);

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
    char **p;

    p = &cred->ctx->sasl_client_res;
    free(*p);
    *p = concat("user=", user, "\1auth=Bearer ", cred->access_token, "\1\1", NULL);
    *res_len = strlen(*p);
    return *p;
}

/*******************************************************************************
 * building URLs and making HTTP requests with libcurl
 */

/*
 * Build null-terminated URL in the array pointed to by s.  If the URL doesn't
 * fit within size (including the terminating null byte), return false without *
 * building the entire URL.  Some of URL may already have been written into the
 * result array in that case.
 */
static bool
make_query_url(char *s, size_t size, CURL *curl, const char *base_url, ...)
{
    bool result = false;
    size_t len;
    char *prefix;
    va_list ap;
    const char *name;

    if (base_url == NULL) {
        len = 0;
        prefix = "";
    } else {
        len = strlen(base_url);
        if (len > size - 1) /* Less one for NUL. */
            return false;
        strcpy(s, base_url);
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

    result = true;

  out:
    va_end(ap);
    return result;
}

static int
debug_callback(CURL *handle, curl_infotype type, char *data,
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
        putc('\n', fp);
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
      ctx->too_big = true;
      return 0;
    }

    memcpy(ctx->res_body + ctx->res_len, ptr, size);
    ctx->res_len = new_len;

    return size;
}

static bool
post(struct curl_ctx *ctx, const char *url, const char *req_body)
{
    CURL *curl = ctx->curl;
    CURLcode status;

    ctx->too_big = false;
    ctx->res_len = 0;

    if (ctx->log != NULL) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, (long)1);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, ctx->log);
    }

    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);

    if (has_prefix(url, "http://127.0.0.1:")) {
        /* Hack:  on Cygwin, curl doesn't fail to connect with ECONNREFUSED.
           Instead, it waits to timeout.  So set a really short timeout, but
           just on localhost (for convenience of the user, and the test
           suite). */
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    }

    status = curl_easy_perform(curl);
    /* first check for error from callback */
    if (ctx->too_big) {
        return false;
    }
    /* now from curl */
    if (status != CURLE_OK) {
        return false;
    }

    return curl_easy_getinfo(curl,
            CURLINFO_RESPONSE_CODE, &ctx->res_code) == CURLE_OK &&
        curl_easy_getinfo(curl,
            CURLINFO_CONTENT_TYPE, &ctx->content_type) == CURLE_OK;
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
static bool
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

    return r > 0;
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
    bool is_key = true;

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
            is_key = true;
            continue;
        }
        if (is_key) {
            is_key = false;
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
        is_key = true;
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
static bool
get_json_strings(const char *input, size_t input_len, FILE *log, ...)
{
    bool result = false;
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

    result = true;

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
