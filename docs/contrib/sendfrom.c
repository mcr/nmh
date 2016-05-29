/*
 * sendfrom.c -- postproc that selects user, and then mail server, from draft
 *
 * Author:  David Levine <levinedl@acm.org>
 *
 * This program fits between send(1) and post(1), as a postproc.  It makes
 * up for the facts that send doesn't parse the message draft and post
 * doesn't load the profile.
 *
 * To use:
 *
 * 1) Add profile entries of the form:
 *
 *        sendfrom-<email address or domain name>:  <post(1) switches>
 *
 *    The email address is extracted from the From: header line of the message draft.  Multiple
 *    profile entries, with different email addresses or domain names, are supported.  This
 *    allows different switches to post(1), such as -user, to be associated with different email
 *    addresses.  If a domain name is used, it matches all users in that domain.
 *
 *    Example profile entry using OAuth for account hosted by gmail:
 *
 *       sendfrom-gmail_address@example.com: -saslmech xoauth2 -authservice gmail -tls
 *           -server smtp.gmail.com -user gmail_login@example.com
 *
 *    (Indentation indicates a continued line, as supported in MH profiles.)  The username
 *    need not be the same as the sender address, which was extracted from the From: header line.
 *
 *    Example profile entries that use an nmh credentials file:
 *
 *       credentials: file:nmhcreds
 *       sendfrom-sendgrid_address@example.com: -sasl -tls -server smtp.sendgrid.net
 *       sendfrom-outbound.att.net: -sasl -initialtls -server outbound.att.net -port 465
 *       sendfrom-fastmail.com: -initialtls -sasl -saslmech LOGIN
 *           -server smtps-proxy.messagingengine.com -port 80
 *
 *    where nmhcreds is in the user's nmh directory (from the Path profile component) and contains:
 *
 *       machine smtp.sendgrid.net
 *           login sendgrid_login@example.com
 *           password ********
 *       machine outbound.att.net
 *           login att_login@example.com
 *           password ********
 *       machine smtps-proxy.messagingengine.com
 *           login fastmail_login@example.com
 *           password ********
 *
 * 2) To enable, add a line like this to your profile:
 *
 *        postproc: <docdir>/contrib/sendfrom
 *
 *    with <docdir> expanded.  This source file is in docdir.  Also, "mhparam docdir" will show it.
 *
 * For more information on authentication to mail servers, see mhlogin(1) for OAuth
 * services, and mh-profile(5) for login credentials.
 *
 * This program follows these steps:
 * 1) Loads profile.
 * 2) Saves command line arguments in vec.
 * 3) Extracts address and domain name from From: header line in draft.
 * 4) Extracts any address or host specific switches to post(1) from profile.  Appends
      to vec, along with implied switches, such as OAUTH access token.
 * 5) Calls post, or with -dryrun, echos how it would be called.
 */

#include <h/mh.h>
#include <h/fmt_scan.h>
#include <h/fmt_compile.h>
#include <h/utils.h>

#ifdef OAUTH_SUPPORT
#include <h/oauth.h>

static int setup_oauth_params(char *vec[], int *, int, const char **);
#endif /* OAUTH_SUPPORT */

extern char *mhlibexecdir;      /* from config.c */

static int get_from_header_info(const char *, const char **, const char **, const char **);
static const char *get_message_header_info(FILE *, char *);
static void merge_profile_entry(const char *, const char *, char *vec[], int *vecp);
static int run_program(char *, char **);

int
main(int argc, char **argv) {
    /* Make sure that nmh's post gets called by specifying its full path. */
    char *realpost = concat(mhlibexecdir, "/", "post", NULL);
    int whom = 0, dryrun = 0, snoop = 0;
    char **arguments, **argp;
    char *program, **vec;
    int vecp = 0;
    char *msg = NULL;

    /* Load profile. */
    if (nmh_init(argv[0], 1)) { return NOTOK; }

    /* Save command line arguments in vec, except for the last argument, if it's the draft file path. */
    vec = argsplit(realpost, &program, &vecp);
    for (argp = arguments = getarguments(invo_name, argc, argv, 1); *argp; ++argp) {
        /* Don't pass -dryrun to post by putting it in vec. */
        if (strcmp(*argp, "-dryrun") == 0) {
            dryrun = 1;
        } else {
            vec[vecp++] = getcpy(*argp);

            if (strcmp(*argp, "-snoop") == 0) {
                snoop = 1;
            } else if (strcmp(*argp, "-whom") == 0) {
                whom = 1;
            } else {
                /* Need to keep filename, from last arg (though it's not used below with -whom). */
                msg = *argp;
            }
        }
    }

    free(arguments);

    if (! whom  &&  msg == NULL) {
        adios(NULL, "requires exactly 1 filename argument");
    } else {
        char **vp;

        /* With post, but without -whom, need to keep filename as last arg. */
        if (! whom) {
            /* At this point, vecp points to the next argument to be added.  Back it up to
               remove the last one, which is the filename.  Add it back at the end of vec below. */
            free(vec[--vecp]);
        }
        /* Null terminate vec because it's used below. */
        vec[vecp] = NULL;

        if (! whom) {
            /* Some users might need the switch from the profile entry with -whom, but that would
               require figuring out which command line argument has the filename.  With -whom, it's
               not necessarily the last one. */

            const char *addr, *host;
            const char *message;

            /* Extract address and host from From: header line in draft. */
            if (get_from_header_info(msg, &addr, &host, &message) != OK) {
                adios(msg, (char *) message);
            }

            /* Merge in any address or host specific switches to post(1) from profile. */
            merge_profile_entry(addr, host, vec, &vecp);
            free((void *) host);
            free((void *) addr);
        }

        vec[vecp] = NULL;
        for (vp = vec; *vp; ++vp) {
            if (strcmp(*vp, "xoauth2") == 0) {
#ifdef OAUTH_SUPPORT
                const char *message;

                if (setup_oauth_params(vec, &vecp, snoop, &message) != OK) {
                    adios(NULL, message);
                }
                break;
#else
                NMH_UNUSED(snoop);
                NMH_UNUSED(vp);
                adios(NULL, "sendfrom built without OAUTH_SUPPORT, so -saslmech xoauth2 is not supported");
#endif /* OAUTH_SUPPORT */
            }
        }

        if (! whom) {
            /* Append filename as last non-null vec entry. */
            vec[vecp++] = getcpy(msg);
            vec[vecp] = NULL;
        }

        /* Call post, or with -dryrun, echo how it would be called. */
        if (dryrun) {
            /* Show the program name, because run_program() won't. */
            printf("%s ", realpost);
            fflush(stdout);
        }
        if (run_program(dryrun ? "echo" : realpost, vec) != OK) {
            adios(realpost, "failure in ");
        }

        arglist_free(program, vec);
        free(realpost);
    }

    return 0;
}



#ifdef OAUTH_SUPPORT
/*
 * For XOAUTH2, append access token, from mh_oauth_do_xoauth(), for the user to vec.
 */
static
int
setup_oauth_params(char *vec[], int *vecp, int snoop, const char **message) {
    const char *saslmech = NULL, *user = NULL, *auth_svc = NULL;
    int i;

    /* Make sure we have all the information we need. */
    for (i = 1; i < *vecp; ++i) {
        /* Don't support abbreviated switches, to avoid collisions in the future if new ones
           are added. */
        if (! strcmp(vec[i-1], "-saslmech")) {
            saslmech = vec[i];
        } else if (! strcmp(vec[i-1], "-user")) {
            user = vec[i];
        } else if (! strcmp(vec[i-1], "-authservice")) {
            auth_svc = vec[i];
        }
    }

    if (auth_svc == NULL) {
        if (saslmech  &&  ! strcasecmp(saslmech, "xoauth2")) {
            *message = "must specify -authservice with -saslmech xoauth2";
            return NOTOK;
        }
    } else {
        if (user == NULL) {
            *message = "must specify -user with -saslmech xoauth2";
            return NOTOK;
        }

        vec[(*vecp)++] = getcpy("-authservice");
        if (saslmech  &&  ! strcasecmp(saslmech, "xoauth2")) {
            vec[(*vecp)++] = mh_oauth_do_xoauth(user, auth_svc, snoop ? stderr : NULL);
        } else {
            vec[(*vecp)++] = getcpy(auth_svc);
        }
    }

    return 0;
}
#endif /* OAUTH_SUPPORT */


/*
 * Extract user and domain from From: header line in draft.
 */
static
int
get_from_header_info(const char *filename, const char **addr, const char **host, const char **message) {
    struct stat st;
    FILE *in;

    if (stat (filename, &st) == NOTOK) {
        *message = "unable to stat draft file";
        return NOTOK;
    }

    if ((in = fopen(filename, "r")) != NULL) {
        char *addrformat = "%(addr{from})", *hostformat = "%(host{from})";

        if ((*addr = get_message_header_info(in, addrformat)) == NULL) {
            *message = "unable to find From: address in";
            return NOTOK;
        }
        rewind(in);

        if ((*host = get_message_header_info(in, hostformat)) == NULL) {
            fclose(in);
            *message = "unable to find From: host in";
            return NOTOK;
        }
        fclose(in);

        return OK;
    } else {
        *message = "unable to open";
        return NOTOK;
    }
}


/*
 * Get formatted information from header of a message.
 * Adapted from process_single_file() in uip/fmttest.c.
 */
static
const char *
get_message_header_info(FILE *in, char *format) {
    int dat[5];
    struct format *fmt;
    struct stat st;
    int parsing_header;
    m_getfld_state_t gstate = 0;
    charstring_t buffer = charstring_create(0);
    char *retval;

    dat[0] = dat[1] = dat[4] = 0;
    dat[2] = fstat(fileno(in), &st) == 0  ?  st.st_size  :  0;
    dat[3] = INT_MAX;

    (void) fmt_compile(new_fs(NULL, NULL, format), &fmt, 1);

    /*
     * Read in the message and process the header.
     */
    parsing_header = 1;
    do {
        char name[NAMESZ], rbuf[NMH_BUFSIZ];
        int bufsz = sizeof rbuf;
        int state = m_getfld(&gstate, name, rbuf, &bufsz, in);

        switch (state) {
        case FLD:
        case FLDPLUS: {
            int bucket = fmt_addcomptext(name, rbuf);

            if (bucket != -1) {
                while (state == FLDPLUS) {
                    bufsz = sizeof rbuf;
                    state = m_getfld(&gstate, name, rbuf, &bufsz, in);
                    fmt_appendcomp(bucket, name, rbuf);
                }
            }

            while (state == FLDPLUS) {
                bufsz = sizeof rbuf;
                state = m_getfld(&gstate, name, rbuf, &bufsz, in);
            }
            break;
        }
        default:
            parsing_header = 0;
        }
    } while (parsing_header);
    m_getfld_state_destroy(&gstate);

    fmt_scan(fmt, buffer, INT_MAX, dat, NULL);
    fmt_free(fmt, 1);

    /* Trim trailing newline, if any. */
    retval = rtrim(charstring_buffer_copy((buffer)));
    charstring_free(buffer);

    return retval;
}


/*
 * Look in profile for entry corresponding to addr or host, and add its contents to vec.
 *
 * Could do some of this automatically, by looking for:
 * 1) access-$(mbox{from}) in oauth-svc file using mh_oauth_cred_load(), which isn't
 *    static and doesn't have side effects; free the result with mh_oauth_cred_free())
 * 2) machine $(mbox{from}) in creds
 * If no -server passed in from profile or commandline, could use smtp.<svc>.com for gmail,
 * but that might not generalize for other svcs.
 */
static
void
merge_profile_entry(const char *addr, const char *host, char *vec[], int *vecp) {
    char *addr_entry = concat("sendfrom-", addr, NULL);
    char *profile_entry = context_find(addr_entry);

    free(addr_entry);
    if (profile_entry == NULL) {
        /* No entry for the user.  Look for one for the host. */
        char *host_entry = concat("sendfrom-", host, NULL);

        profile_entry = context_find(host_entry);
        free(host_entry);
    }

    /* Use argsplit() to do the real work of splitting the args in the profile entry. */
    if (profile_entry  &&  strlen(profile_entry) > 0) {
        int profile_vecp;
        char *file;
        char **profile_vec = argsplit(profile_entry, &file, &profile_vecp);
        int i;

        for (i = 0; i < profile_vecp; ++i) {
            vec[(*vecp)++] = getcpy(profile_vec[i]);
        }

        arglist_free(file, profile_vec);
    }
}


/*
 * Fork and exec.
 */
static
int
run_program(char *realpost, char **vec) {
    pid_t child_id;
    int i;

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; ++i) { sleep(5); }

    switch (child_id) {
    case -1:
        /* oops -- fork error */
        return NOTOK;
    case 0:
        (void) execvp(realpost, vec);
        fprintf(stderr, "unable to exec ");
        perror(realpost);
        _exit(-1);
    default:
        if (pidXwait(child_id, realpost)) {
            return NOTOK;
        }
    }

    return OK;
}
