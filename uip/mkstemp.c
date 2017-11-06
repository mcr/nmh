/* mkstemp.c -- create a temporary file
 *
 * This code is Copyright (c) 2014 by the authors of nmh.
 * See the COPYRIGHT file in the root directory of the nmh
 * distribution for complete copyright information.
 */

/* define NMH to 0 to remove dependencies on nmh. */
#ifndef NMH
#   define NMH 1
#endif /* ! NMH */

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif /* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#if ! defined HAVE_MKSTEMPS
#   define HAVE_MKSTEMPS 0
#endif /* ! HAVE_MKSTEMPS */

static char *build_template(const char *, const char *, const char *);
static void process_args(int, char **, const char **, const char **, const char **);

/*
 * Use a template of the form:
 *     [directory/][prefix]XXXXXX[suffix]
 * where some of those named components might be null.  suffix is only
 * supported if HAVE_MKSTEMPS.
 */

int
main(int argc, char *argv[])
{
    const char *directory = "", *prefix = "", *suffix = "";
    size_t suffix_len;
    int fd;
    char *template;

    process_args(argc, argv, &directory, &prefix, &suffix);
    if ((template = build_template(directory, prefix, suffix)) == NULL) {
        return 1;
    }

    if ((suffix_len = strlen(suffix)) > 0) {
#       if HAVE_MKSTEMPS
        if ((fd = mkstemps(template, suffix_len)) < 0) { perror("mkstemps"); }
#       else  /* ! HAVE_MKSTEMPS */
        fd = 0;
#       endif /* ! HAVE_MKSTEMPS */
    } else {
        if ((fd = mkstemp(template)) < 0) { perror("mkstemp"); }
    }

    if (fd >= 0) {
        (void) puts(template);
        (void) close(fd);
    }

    free(template);

    return fd >= 0  ?  0  :  1;
}


static char *
build_template(const char *directory, const char *prefix, const char *suffix)
{
    const char pattern[] = "XXXXXX";
    size_t len, directory_len, pathsep_len, prefix_len, suffix_len;
    char *template;

    directory_len = strlen(directory);
    if (directory_len > 0) {
        pathsep_len = 1;
        if (directory[directory_len - 1] == '/') {
            /* Will insert a '/' separately, so truncate the one provided
               in the directory name. */
            --directory_len;
        }
    } else {
        pathsep_len = 0;
    }
    prefix_len = strlen(prefix);
    suffix_len = strlen(suffix);
    /* sizeof pattern includes its final NULL, so don't add another. */
    len = directory_len  +  pathsep_len  +  prefix_len  +   sizeof pattern  +
        suffix_len;

    if ((template = malloc(len))) {
        char *tp = template;

        (void) strncpy(tp, directory, directory_len);
        tp += directory_len;

        if (pathsep_len == 1) { *tp++ = '/'; }

        (void) strncpy(tp, prefix, prefix_len);
        tp += prefix_len;

        (void) strncpy(tp, pattern, sizeof pattern - 1);
        tp += sizeof pattern - 1;

        (void) strncpy(tp, suffix, suffix_len);
        /* tp += suffix_len; */

        template[len-1] = '\0';

        return template;
    }

    perror("malloc");
    return NULL;
}


#if NMH
#include "h/mh.h"
#include "sbr/ambigsw.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include "h/done.h"
#include "h/utils.h"

#if HAVE_MKSTEMPS
#   define MHFIXMSG_SWITCHES \
    X("directory", 0, DIRECTORYSW) \
    X("prefix", 0, PREFIXSW) \
    X("suffix", 0, SUFFIXSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW)
#else  /* ! HAVE_MKSTEMPS */
#   define MHFIXMSG_SWITCHES \
    X("directory", 0, DIRECTORYSW) \
    X("prefix", 0, PREFIXSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW)
#endif /* ! HAVE_MKSTEMPS */

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHFIXMSG);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHFIXMSG, switches);
#undef X

static void
process_args(int argc, char **argv, const char **directory,
             const char **prefix, const char **suffix)
{
    char **argp, **arguments, *cp, buf[100];
#   if ! HAVE_MKSTEMPS
    NMH_UNUSED(suffix);
#   endif /* ! HAVE_MKSTEMPS */

    if (nmh_init(argv[0], true, false)) { done(1); }
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Parse arguments
     */
    while ((cp = *argp++)) {
        if (*cp == '-') {
            switch (smatch(++cp, switches)) {
            case AMBIGSW:
                ambigsw(cp, switches);
                done(1);
            case UNKWNSW:
                inform("-%s unknown", cp);
                (void) snprintf(buf, sizeof buf, "%s [switches]", invo_name);
                print_help(buf, switches, 1);
                done(1);
            case HELPSW:
                (void) snprintf(buf, sizeof buf, "%s [switches]", invo_name);
                print_help(buf, switches, 1);
                done(OK);
            case VERSIONSW:
                print_version(invo_name);
                done(OK);

            case DIRECTORYSW:
                /* Allow the directory to start with '-'. */
                if ((cp = *argp++) == NULL) {
                    die("missing argument to %s", argp[-2]);
                }
                *directory = cp;
                continue;

            case PREFIXSW:
                /* Allow the prefix to start with '-'. */
                if ((cp = *argp++) == NULL) {
                    die("missing argument to %s", argp[-2]);
                }
                *prefix = cp;
                continue;

#           if HAVE_MKSTEMPS
            case SUFFIXSW:
                /* Allow the suffix to start with '-'. */
                if ((cp = *argp++) == NULL) {
                    die("missing argument to %s", argp[-2]);
                }
                *suffix = cp;
                continue;
#           endif /* HAVE_MKSTEMPS */
            }
        }
    }
}
#else  /* ! NMH */
static void
process_args(int argc, char **argv, const char **directory,
             const char **prefix, const char **suffix)
{
#   if HAVE_MKSTEMPS
    const char usage[] =
        "usage: %s [-h] [-d directory] [-p prefix] [-s suffix]\n";
    const char optstring[] = "d:hp:s:";
#   else  /* ! HAVE_MKSTEMPS */
    const char usage[] = "usage: %s [-h] [-d directory] [-p prefix]\n";
    const char optstring[] = "d:hp:";
#   endif /* ! HAVE_MKSTEMPS */
    int opt;

    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
        case 'd':
            *directory = optarg;
            break;
        case 'p':
            *prefix = optarg;
            break;
        case 's':
            *suffix = optarg;
            break;
        case 'h':
            (void) printf(usage, argv[0]);
            exit(0);
        default:
            (void) fprintf(stderr, usage, argv[0]);
            exit(1);
        }
    }
}
#endif /* ! NMH */
