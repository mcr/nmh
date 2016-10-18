/*
 * mhfixmsg.c -- rewrite a message with various transformations
 *
 * This code is Copyright (c) 2002 and 2013, by the authors of nmh.
 * See the COPYRIGHT file in the root directory of the nmh
 * distribution for complete copyright information.
 */

#include <h/mh.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/utils.h>
#include <h/signals.h>
#include <fcntl.h>

#define MHFIXMSG_SWITCHES \
    X("decodetext 8bit|7bit|binary", 0, DECODETEXTSW) \
    X("nodecodetext", 0, NDECODETEXTSW) \
    X("decodetypes", 0, DECODETYPESW) \
    X("crlflinebreaks", 0, CRLFLINEBREAKSSW) \
    X("nocrlflinebreaks", 0, NCRLFLINEBREAKSSW) \
    X("textcharset", 0, TEXTCHARSETSW) \
    X("notextcharset", 0, NTEXTCHARSETSW) \
    X("reformat", 0, REFORMATSW) \
    X("noreformat", 0, NREFORMATSW) \
    X("replacetextplain", 0, REPLACETEXTPLAINSW) \
    X("noreplacetextplain", 0, NREPLACETEXTPLAINSW) \
    X("fixboundary", 0, FIXBOUNDARYSW) \
    X("nofixboundary", 0, NFIXBOUNDARYSW) \
    X("fixcte", 0, FIXCOMPOSITECTESW) \
    X("nofixcte", 0, NFIXCOMPOSITECTESW) \
    X("fixtype mimetype", 0, FIXTYPESW) \
    X("file file", 0, FILESW) \
    X("outfile file", 0, OUTFILESW) \
    X("rmmproc program", 0, RPROCSW) \
    X("normmproc", 0, NRPRCSW) \
    X("changecur", 0, CHGSW) \
    X("nochangecur", 0, NCHGSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHFIXMSG);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHFIXMSG, switches);
#undef X


int verbosw;
int debugsw; /* Needed by mhparse.c. */

#define quitser pipeser

/* mhparse.c */
extern int skip_mp_cte_check;                 /* flag to InitMultiPart */
extern int suppress_bogus_mp_content_warning; /* flag to InitMultiPart */
extern int bogus_mp_content;                  /* flag from InitMultiPart */
/* flags to/from parse_header_attrs */
extern int suppress_extraneous_trailing_semicolon_warning;
extern int extraneous_trailing_semicolon;

/* mhoutsbr.c */
int output_message (CT, char *);

/* mhmisc.c */
void flush_errors (void);

/* mhfree.c */
extern CT *cts;
void freects_done (int) NORETURN;

/*
 * static prototypes
 */
typedef struct fix_transformations {
    int fixboundary;
    int fixcompositecte;
    svector_t fixtypes;
    int reformat;
    int replacetextplain;
    int decodetext;
    char *decodetypes;
    /* Whether to use CRLF linebreaks, per RFC 2046 Sec. 4.1.1, par.1. */
    int lf_line_endings;
    char *textcharset;
} fix_transformations;

int mhfixmsgsbr (CT *, const fix_transformations *, char *);
static int fix_boundary (CT *, int *);
static int copy_input_to_output (const char *, const char *);
static int get_multipart_boundary (CT, char **);
static int replace_boundary (CT, char *, char *);
static int fix_types (CT, svector_t, int *);
static char *replace_substring (char **, const char *, const char *);
static char *remove_parameter (char *, const char *);
static int fix_composite_cte (CT, int *);
static int set_ce (CT, int);
static int ensure_text_plain (CT *, CT, int *, int);
static int find_textplain_sibling (CT, int, int *);
static int insert_new_text_plain_part (CT, int, CT);
static CT build_text_plain_part (CT);
static int insert_into_new_mp_alt (CT *, int *);
static CT divide_part (CT);
static void copy_ctinfo (CI, CI);
static int decode_part (CT);
static int reformat_part (CT, char *, char *, char *, int);
static CT build_multipart_alt (CT, CT, int, int);
static int boundary_in_content (FILE **, char *, const char *);
static void transfer_noncontent_headers (CT, CT);
static int set_ct_type (CT, int type, int subtype, int encoding);
static int decode_text_parts (CT, int, const char *, int *);
static int should_decode(const char *, const char *, const char *);
static int content_encoding (CT, const char **);
static int strip_crs (CT, int *);
static void update_cte (CT);
static int least_restrictive_encoding (CT);
static int less_restrictive (int, int);
static int convert_charsets (CT, char *, int *);
static int fix_always (CT, int *);
static int fix_filename_param (char *, char *, PM *, PM *);
static int fix_filename_encoding (CT);
static int write_content (CT, const char *, char *, int, int);
static void set_text_ctparams(CT, char *, int);
static int remove_file (const char *);
static void report (char *, char *, char *, char *, ...);
static void pipeser (int);


int
main (int argc, char **argv) {
    int msgnum;
    char *cp, *file = NULL, *folder = NULL;
    char *maildir, buf[100], *outfile = NULL;
    char **argp, **arguments;
    struct msgs_array msgs = { 0, 0, NULL };
    struct msgs *mp = NULL;
    CT *ctp;
    FILE *fp;
    int using_stdin = 0;
    int chgflag = 1;
    int status = OK;
    fix_transformations fx;
    fx.reformat = fx.fixcompositecte = fx.fixboundary = 1;
    fx.fixtypes = NULL;
    fx.replacetextplain = 0;
    fx.decodetext = CE_8BIT;
    fx.decodetypes = "text,application/ics";  /* Default, per man page. */
    fx.lf_line_endings = 0;
    fx.textcharset = NULL;

    if (nmh_init(argv[0], 2)) { return 1; }

    done = freects_done;

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Parse arguments
     */
    while ((cp = *argp++)) {
        if (*cp == '-') {
            switch (smatch (++cp, switches)) {
            case AMBIGSW:
                ambigsw (cp, switches);
                done (1);
            case UNKWNSW:
                adios (NULL, "-%s unknown", cp);

            case HELPSW:
                snprintf (buf, sizeof buf, "%s [+folder] [msgs] [switches]",
                        invo_name);
                print_help (buf, switches, 1);
                done (0);
            case VERSIONSW:
                print_version(invo_name);
                done (0);

            case DECODETEXTSW:
                if (! (cp = *argp++)  ||  *cp == '-') {
                    adios (NULL, "missing argument to %s", argp[-2]);
                }
                if (! strcasecmp (cp, "8bit")) {
                    fx.decodetext = CE_8BIT;
                } else if (! strcasecmp (cp, "7bit")) {
                    fx.decodetext = CE_7BIT;
                } else if (! strcasecmp (cp, "binary")) {
                    fx.decodetext = CE_BINARY;
                } else {
                    adios (NULL, "invalid argument to %s", argp[-2]);
                }
                continue;
            case NDECODETEXTSW:
                fx.decodetext = 0;
                continue;
            case DECODETYPESW:
                if (! (cp = *argp++)  ||  *cp == '-') {
                    adios (NULL, "missing argument to %s", argp[-2]);
                }
                fx.decodetypes = cp;
                continue;
            case CRLFLINEBREAKSSW:
                fx.lf_line_endings = 0;
                continue;
            case NCRLFLINEBREAKSSW:
                fx.lf_line_endings = 1;
                continue;
            case TEXTCHARSETSW:
                if (! (cp = *argp++) || (*cp == '-' && cp[1])) {
                    adios (NULL, "missing argument to %s", argp[-2]);
                }
                fx.textcharset = cp;
                continue;
            case NTEXTCHARSETSW:
                fx.textcharset = 0;
                continue;
            case FIXBOUNDARYSW:
                fx.fixboundary = 1;
                continue;
            case NFIXBOUNDARYSW:
                fx.fixboundary = 0;
                continue;
            case FIXCOMPOSITECTESW:
                fx.fixcompositecte = 1;
                continue;
            case NFIXCOMPOSITECTESW:
                fx.fixcompositecte = 0;
                continue;
            case FIXTYPESW:
                if (! (cp = *argp++) || (*cp == '-' && cp[1])) {
                    adios (NULL, "missing argument to %s", argp[-2]);
                }
                if (! strncasecmp (cp, "multipart/", 10)  ||
                    ! strncasecmp (cp, "message/", 8)) {
                    adios (NULL, "-fixtype %s not allowed", cp);
                } else if (! strchr (cp, '/')) {
                    adios (NULL, "-fixtype requires type/subtype");
                }
                if (fx.fixtypes == NULL) { fx.fixtypes = svector_create (10); }
                svector_push_back (fx.fixtypes, cp);
                continue;
            case REFORMATSW:
                fx.reformat = 1;
                continue;
            case NREFORMATSW:
                fx.reformat = 0;
                continue;
            case REPLACETEXTPLAINSW:
                fx.replacetextplain = 1;
                continue;
            case NREPLACETEXTPLAINSW:
                fx.replacetextplain = 0;
                continue;
            case FILESW:
                if (! (cp = *argp++) || (*cp == '-' && cp[1])) {
                    adios (NULL, "missing argument to %s", argp[-2]);
                }
                file = *cp == '-'  ?  add (cp, NULL)  :  path (cp, TFILE);
                continue;
            case OUTFILESW:
                if (! (cp = *argp++) || (*cp == '-' && cp[1])) {
                    adios (NULL, "missing argument to %s", argp[-2]);
                }
                outfile = *cp == '-'  ?  add (cp, NULL)  :  path (cp, TFILE);
                continue;
            case RPROCSW:
                if (!(rmmproc = *argp++) || *rmmproc == '-') {
                    adios (NULL, "missing argument to %s", argp[-2]);
                }
                continue;
            case NRPRCSW:
                rmmproc = NULL;
                continue;
            case CHGSW:
                chgflag = 1;
                continue;
            case NCHGSW:
                chgflag = 0;
                continue;
            case VERBSW:
                verbosw = 1;
                continue;
            case NVERBSW:
                verbosw = 0;
                continue;
            }
        }
        if (*cp == '+' || *cp == '@') {
            if (folder) {
                adios (NULL, "only one folder at a time!");
            } else {
                folder = pluspath (cp);
            }
        } else {
            if (*cp == '/') {
                /* Interpret a full path as a filename, not a message. */
                file = add (cp, NULL);
            } else {
                app_msgarg (&msgs, cp);
            }
        }
    }

    SIGNAL (SIGQUIT, quitser);
    SIGNAL (SIGPIPE, pipeser);

    /*
     * Read the standard profile setup
     */
    if ((fp = fopen (cp = etcpath ("mhn.defaults"), "r"))) {
        readconfig ((struct node **) 0, fp, cp, 0);
        fclose (fp);
    }

    suppress_bogus_mp_content_warning = skip_mp_cte_check = 1;
    suppress_extraneous_trailing_semicolon_warning = 1;

    if (! context_find ("path")) {
        free (path ("./", TFOLDER));
    }

    if (file && msgs.size) {
        adios (NULL, "cannot specify msg and file at same time!");
    }

    /*
     * check if message is coming from file
     */
    if (file) {
        /* If file is stdin, create a tmp file name before parse_mime()
           has a chance, because it might put in on a different
           filesystem than the output file.  Instead, put it in the
           user's preferred tmp directory. */
        CT ct;

        if (! strcmp ("-", file)) {
            int fd;
            char *cp;

            using_stdin = 1;

            if ((cp = m_mktemp2 (NULL, invo_name, &fd, NULL)) == NULL) {
                adios (NULL, "unable to create temporary file in %s",
                       get_temp_dir());
            } else {
                free (file);
                file = add (cp, NULL);
                cpydata (STDIN_FILENO, fd, "-", file);
            }

            if (close (fd)) {
                (void) m_unlink (file);
                adios (NULL, "failed to write temporary file");
            }
        }

        cts = mh_xcalloc(2, sizeof *cts);
        ctp = cts;

        if ((ct = parse_mime (file))) {
            set_text_ctparams(ct, fx.decodetypes, fx.lf_line_endings);
            *ctp++ = ct;
        } else {
            advise (NULL, "unable to parse message from file %s", file);
            status = NOTOK;

            /* If there's an outfile, pass the input message unchanged, so the message won't
               get dropped from a pipeline. */
            if (outfile) {
                /* Something went wrong.  Output might be expected, such as if this were run
                   as a filter.  Just copy the input to the output. */
                if (copy_input_to_output (file, outfile) != OK) {
                    advise (NULL, "unable to copy message to %s, it might be lost\n", outfile);
                }
            }
        }
    } else {
        /*
         * message(s) are coming from a folder
         */
        CT ct;

        if (! msgs.size) {
            app_msgarg(&msgs, "cur");
        }
        if (! folder) {
            folder = getfolder (1);
        }
        maildir = m_maildir (folder);

        if (chdir (maildir) == NOTOK) {
            adios (maildir, "unable to change directory to");
        }

        /* read folder and create message structure */
        if (! (mp = folder_read (folder, 1))) {
            adios (NULL, "unable to read folder %s", folder);
        }

        /* check for empty folder */
        if (mp->nummsg == 0) {
            adios (NULL, "no messages in %s", folder);
        }

        /* parse all the message ranges/sequences and set SELECTED */
        for (msgnum = 0; msgnum < msgs.size; msgnum++)
            if (! m_convert (mp, msgs.msgs[msgnum])) {
                done (1);
            }
        seq_setprev (mp);       /* set the previous-sequence */

        cts = mh_xcalloc(mp->numsel + 1, sizeof *cts);
        ctp = cts;

        for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++) {
            if (is_selected(mp, msgnum)) {
                char *msgnam;

                msgnam = m_name (msgnum);
                if ((ct = parse_mime (msgnam))) {
                    set_text_ctparams(ct, fx.decodetypes, fx.lf_line_endings);
                    *ctp++ = ct;
                } else {
                    advise (NULL, "unable to parse message %s", msgnam);
                    status = NOTOK;

                    /* If there's an outfile, pass the input message unchanged, so the message won't
                       get dropped from a pipeline. */
                    if (outfile) {
                        /* Something went wrong.  Output might be expected, such as if this were run
                           as a filter.  Just copy the input to the output. */
                        const char *input_filename = path (msgnam, TFILE);

                        if (copy_input_to_output (input_filename, outfile) != OK) {
                            advise (NULL, "unable to copy message to %s, it might be lost\n", outfile);
                        }
                    }
                }
            }
        }

        if (chgflag) {
            seq_setcur (mp, mp->hghsel);  /* update current message */
        }
        seq_save (mp);                    /* synchronize sequences  */
        context_replace (pfolder, folder);/* update current folder  */
        context_save ();                  /* save the context file  */
    }

    if (*cts) {
        for (ctp = cts; *ctp; ++ctp) {
            status += mhfixmsgsbr (ctp, &fx, outfile);

            if (using_stdin) {
                (void) m_unlink (file);

                if (! outfile) {
                    /* Just calling m_backup() unlinks the backup file. */
                    (void) m_backup (file);
                }
            }
        }
    } else {
        status = 1;
    }

    if (fx.fixtypes != NULL) { svector_free (fx.fixtypes); }
    free (outfile);
    free (file);
    free (folder);
    free (arguments);

    /* done is freects_done, which will clean up all of cts. */
    done (status);
    return NOTOK;
}


/*
 * Apply transformations to one message.
 */
int
mhfixmsgsbr (CT *ctp, const fix_transformations *fx, char *outfile) {
    /* Store input filename in case one of the transformations, i.e.,
       fix_boundary(), rewrites to a tmp file. */
    char *input_filename = add ((*ctp)->c_file, NULL);
    int modify_inplace = 0;
    int message_mods = 0;
    int status = OK;

    if (outfile == NULL) {
        modify_inplace = 1;

        if ((*ctp)->c_file) {
            char *tempfile;
            if ((tempfile = m_mktemp2 (NULL, invo_name, NULL, NULL)) == NULL) {
                adios (NULL, "unable to create temporary file in %s",
                       get_temp_dir());
            }
            outfile = add (tempfile, NULL);
        } else {
            adios (NULL, "missing both input and output filenames\n");
        }
    }

    reverse_alternative_parts (*ctp);
    status = fix_always (*ctp, &message_mods);
    if (status == OK  &&  fx->fixboundary) {
        status = fix_boundary (ctp, &message_mods);
    }
    if (status == OK  && fx->fixtypes != NULL) {
        status = fix_types (*ctp, fx->fixtypes, &message_mods);
    }
    if (status == OK  &&  fx->fixcompositecte) {
        status = fix_composite_cte (*ctp, &message_mods);
    }
    if (status == OK  &&  fx->reformat) {
        status =
            ensure_text_plain (ctp, NULL, &message_mods, fx->replacetextplain);
    }
    if (status == OK  &&  fx->decodetext) {
        status = decode_text_parts (*ctp, fx->decodetext, fx->decodetypes,
                                    &message_mods);
        update_cte (*ctp);
    }
    if (status == OK  &&  fx->textcharset != NULL) {
        status = convert_charsets (*ctp, fx->textcharset, &message_mods);
    }

    if (status == OK  &&  ! (*ctp)->c_umask) {
        /* Set the umask for the contents file.  This currently
           isn't used but just in case it is in the future. */
        struct stat st;

        if (stat ((*ctp)->c_file, &st) != NOTOK) {
            (*ctp)->c_umask = ~(st.st_mode & 0777);
        } else {
            (*ctp)->c_umask = ~m_gmprot();
        }
    }

    /*
     * Write the content to a file
     */
    if (status == OK) {
        status = write_content (*ctp, input_filename, outfile, modify_inplace,
                                message_mods);
    } else if (! modify_inplace) {
        /* Something went wrong.  Output might be expected, such
           as if this were run as a filter.  Just copy the input
           to the output. */
        if (copy_input_to_output (input_filename, outfile) != OK) {
            advise (NULL, "unable to copy message to %s, it might be lost\n", outfile);
        }
    }

    if (modify_inplace) {
        if (status != OK) { (void) m_unlink (outfile); }
        free (outfile);
        outfile = NULL;
    }

    free (input_filename);

    return status;
}


/*
 * Copy input message to output.  Assumes not modifying in place, so this
 * might be running as part of a pipeline.
 */
static int
copy_input_to_output (const char *input_filename, const char *output_filename) {
    int in = open (input_filename, O_RDONLY);
    int out = strcmp (output_filename, "-")
        ?  open (output_filename, O_WRONLY | O_CREAT, m_gmprot ())
        :  STDOUT_FILENO;
    int status = OK;

    if (in != -1  &&  out != -1) {
        cpydata (in, out, input_filename, output_filename);
    } else {
        status = NOTOK;
    }

    close (out);
    close (in);

    return status;
}


/*
 * Fix mismatched outer level boundary.
 */
static int
fix_boundary (CT *ct, int *message_mods) {
    struct multipart *mp;
    int status = OK;

    if (ct  &&  (*ct)->c_type == CT_MULTIPART  &&  bogus_mp_content) {
        mp = (struct multipart *) (*ct)->c_ctparams;

        /*
         * 1) Get boundary at end of part.
         * 2) Get boundary at beginning of part and compare to the end-of-part
         *    boundary.
         * 3) Write out contents of ct to tmp file, replacing boundary in
         *    header with boundary from part.  Set c_unlink to 1.
         * 4) Free ct.
         * 5) Call parse_mime() on the tmp file, replacing ct.
         */

        if (mp  &&  mp->mp_start) {
            char *part_boundary;

            if (get_multipart_boundary (*ct, &part_boundary) == OK) {
                char *fixed;

                if ((fixed = m_mktemp2 (NULL, invo_name, NULL, &(*ct)->c_fp))) {
                    if (replace_boundary (*ct, fixed, part_boundary) == OK) {
                        char *filename = add ((*ct)->c_file, NULL);
                        CT fixed_ct;

                        free_content (*ct);
                        if ((fixed_ct = parse_mime (fixed))) {
                            *ct = fixed_ct;
                            (*ct)->c_unlink = 1;

                            ++*message_mods;
                            if (verbosw) {
                                report (NULL, NULL, filename,
                                        "fix multipart boundary");
                            }
                        } else {
                            *ct = NULL;
                            advise (NULL, "unable to parse fixed part");
                            status = NOTOK;
                        }
                        free (filename);
                    } else {
                        advise (NULL, "unable to replace broken boundary");
                        status = NOTOK;
                    }
                } else {
                    advise (NULL, "unable to create temporary file in %s",
                            get_temp_dir());
                    status = NOTOK;
                }

                free (part_boundary);
            } else {
                /* Couldn't fix the boundary.  Report failure so that mhfixmsg
                   doesn't modify the message. */
                status = NOTOK;
            }
        } else {
            /* No multipart struct, even though the content type is
               CT_MULTIPART.  Report failure so that mhfixmsg doesn't modify
               the message. */
            status = NOTOK;
        }
    }

    return status;
}


/*
 * Find boundary at end of multipart.
 */
static int
get_multipart_boundary (CT ct, char **part_boundary) {
    char buffer[BUFSIZ];
    char *end_boundary = NULL;
    off_t begin = (off_t) ct->c_end > (off_t) (ct->c_begin + sizeof buffer)
        ?  (off_t) (ct->c_end - sizeof buffer)
        :  (off_t) ct->c_begin;
    size_t bytes_read;
    int status = OK;

    /* This will fail if the boundary spans fread() calls.  BUFSIZ should
       be big enough, even if it's just 1024, to make that unlikely. */

    /* free_content() will close ct->c_fp. */
    if (! ct->c_fp  &&  (ct->c_fp = fopen (ct->c_file, "r")) == NULL) {
        advise (ct->c_file, "unable to open for reading");
        return NOTOK;
    }

    /* Get boundary at end of multipart. */
    while (begin >= (off_t) ct->c_begin) {
        fseeko (ct->c_fp, begin, SEEK_SET);
        while ((bytes_read = fread (buffer, 1, sizeof buffer, ct->c_fp)) > 0) {
            char *cp = rfind_str (buffer, bytes_read, "--");

            if (cp) {
                char *end;

                /* Trim off trailing "--" and anything beyond. */
                *cp-- = '\0';
                if ((end = rfind_str (buffer, cp - buffer, "\n"))) {
                    if (strlen (end) > 3  &&  *end++ == '\n'  &&
                        *end++ == '-'  &&  *end++ == '-') {
                        end_boundary = add (end, NULL);
                        break;
                    }
                }
            }
        }

        if (! end_boundary  &&  begin > (off_t) (ct->c_begin + sizeof buffer)) {
            begin -= sizeof buffer;
        } else {
            break;
        }
    }

    /* Get boundary at beginning of multipart. */
    if (end_boundary) {
        fseeko (ct->c_fp, ct->c_begin, SEEK_SET);
        while ((bytes_read = fread (buffer, 1, sizeof buffer, ct->c_fp)) > 0) {
            if (bytes_read >= strlen (end_boundary)) {
                char *cp = find_str (buffer, bytes_read, end_boundary);

                if (cp  &&  cp - buffer >= 2  &&  *--cp == '-'  &&
                    *--cp == '-'  &&  (cp > buffer  &&  *--cp == '\n')) {
                    status = OK;
                    break;
                }
            } else {
                /* The start and end boundaries didn't match, or the
                   start boundary doesn't begin with "\n--" (or "--"
                   if at the beginning of buffer).  Keep trying. */
                status = NOTOK;
            }
        }
    } else {
        status = NOTOK;
    }

    if (status == OK) {
        *part_boundary = end_boundary;
    } else {
        *part_boundary = NULL;
        free (end_boundary);
    }

    return status;
}


/*
 * Open and copy ct->c_file to file, replacing the multipart boundary.
 */
static int
replace_boundary (CT ct, char *file, char *boundary) {
    FILE *fpin, *fpout;
    int compnum, state;
    char buf[BUFSIZ], name[NAMESZ];
    char *np, *vp;
    m_getfld_state_t gstate = 0;
    int status = OK;

    if (ct->c_file == NULL) {
        advise (NULL, "missing input filename");
        return NOTOK;
    }

    if ((fpin = fopen (ct->c_file, "r")) == NULL) {
        advise (ct->c_file, "unable to open for reading");
        return NOTOK;
    }

    if ((fpout = fopen (file, "w")) == NULL) {
        fclose (fpin);
        advise (file, "unable to open for writing");
        return NOTOK;
    }

    for (compnum = 1;;) {
        int bufsz = (int) sizeof buf;

        switch (state = m_getfld (&gstate, name, buf, &bufsz, fpin)) {
        case FLD:
        case FLDPLUS:
            compnum++;

            /* get copies of the buffers */
            np = add (name, NULL);
            vp = add (buf, NULL);

            /* if necessary, get rest of field */
            while (state == FLDPLUS) {
                bufsz = sizeof buf;
                state = m_getfld (&gstate, name, buf, &bufsz, fpin);
                vp = add (buf, vp);     /* add to previous value */
            }

            if (strcasecmp (TYPE_FIELD, np)) {
                fprintf (fpout, "%s:%s", np, vp);
            } else {
	    	char *new_ctline, *new_params;

	    	replace_param(&ct->c_ctinfo.ci_first_pm,
			      &ct->c_ctinfo.ci_last_pm, "boundary",
			      boundary, 0);

		new_ctline = concat(" ", ct->c_ctinfo.ci_type, "/",
				    ct->c_ctinfo.ci_subtype, NULL);
		new_params = output_params(strlen(TYPE_FIELD) +
					   strlen(new_ctline) + 1,
					   ct->c_ctinfo.ci_first_pm, NULL, 0);
                fprintf (fpout, "%s:%s%s\n", np, new_ctline,
			 new_params ? new_params : "");
		free(new_ctline);
		if (new_params) {
		    free(new_params);
                }
            }

            free (vp);
            free (np);

            continue;

        case BODY:
            fputs ("\n", fpout);
            /* buf will have a terminating NULL, skip it. */
            if ((int) fwrite (buf, 1, bufsz-1, fpout) < bufsz-1) {
                advise (file, "fwrite");
            }
            continue;

        case FILEEOF:
            break;

        case LENERR:
        case FMTERR:
            advise (NULL, "message format error in component #%d", compnum);
            status = NOTOK;
            break;

        default:
            advise (NULL, "getfld() returned %d", state);
            status = NOTOK;
            break;
        }

        break;
    }

    m_getfld_state_destroy (&gstate);
    fclose (fpout);
    fclose (fpin);

    return status;
}


/*
 * Fix Content-Type header to reflect the content of its part.
 */
static int
fix_types (CT ct, svector_t fixtypes, int *message_mods) {
    int status = OK;

    switch (ct->c_type) {
    case CT_MULTIPART: {
        struct multipart *m = (struct multipart *) ct->c_ctparams;
        struct part *part;

        for (part = m->mp_parts; status == OK  &&  part; part = part->mp_next) {
            status = fix_types (part->mp_part, fixtypes, message_mods);
        }
        break;
    }

    case CT_MESSAGE:
        if (ct->c_subtype == MESSAGE_EXTERNAL) {
            struct exbody *e = (struct exbody *) ct->c_ctparams;

            status = fix_types (e->eb_content, fixtypes, message_mods);
        }
        break;

    default: {
        char **typep, *type;

        if (ct->c_ctinfo.ci_type  &&  ct->c_ctinfo.ci_subtype) {
            for (typep = svector_strs (fixtypes);
                 typep && (type = *typep);
                 ++typep) {
                char *type_subtype =
                    concat (ct->c_ctinfo.ci_type, "/", ct->c_ctinfo.ci_subtype,
                            NULL);

                if (! strcasecmp (type, type_subtype)  &&
                    decode_part (ct) == OK  &&
                    ct->c_cefile.ce_file != NULL) {
                    char *ct_type_subtype = mime_type (ct->c_cefile.ce_file);
                    char *cp;

                    if ((cp = strchr (ct_type_subtype, ';'))) {
                        /* Truncate to remove any parameter list from
                           mime_type () result. */
                        *cp = '\0';
                    }

                    if (strcasecmp (type, ct_type_subtype)) {
                        char *ct_type, *ct_subtype;
                        HF hf;

                        /* The Content-Type header does not match the
                           content, so update these struct Content
                           fields to match:
                           * c_type, c_subtype
                           * c_ctinfo.ci_type, c_ctinfo.ci_subtype
                           * c_ctline
                           */
                        /* Extract type and subtype from type/subtype. */
                        ct_type = mh_xstrdup(ct_type_subtype);
                        if ((cp = strchr (ct_type, '/'))) {
                            *cp = '\0';
                            ct_subtype = mh_xstrdup(++cp);
                        } else {
                            advise (NULL, "missing / in MIME type of %s %s",
                                    ct->c_file, ct->c_partno);
                            free (ct_type);
                            return NOTOK;
                        }

                        ct->c_type = ct_str_type (ct_type);
                        ct->c_subtype = ct_str_subtype (ct->c_type, ct_subtype);

                        free (ct->c_ctinfo.ci_type);
                        ct->c_ctinfo.ci_type = ct_type;
                        free (ct->c_ctinfo.ci_subtype);
                        ct->c_ctinfo.ci_subtype = ct_subtype;
                        if (! replace_substring (&ct->c_ctline, type,
                                                 ct_type_subtype)) {
                            advise (NULL, "did not find %s in %s",
                                    type, ct->c_ctline);
                        }

                        /* Update Content-Type header field. */
                        for (hf = ct->c_first_hf; hf; hf = hf->next) {
                            if (! strcasecmp (TYPE_FIELD, hf->name)) {
                                if (replace_substring (&hf->value, type,
                                                       ct_type_subtype)) {
                                    ++*message_mods;
                                    if (verbosw) {
                                        report (NULL, ct->c_partno, ct->c_file,
                                                "change Content-Type in header "
                                                "from %s to %s",
                                                type, ct_type_subtype);
                                    }
                                    break;
                                } else {
                                    advise (NULL, "did not find %s in %s",
                                            type, hf->value);
                                }
                            }
                        }
                    }
                    free (ct_type_subtype);
                }
                free (type_subtype);
            }
        }
    }}

    return status;
}


/*
 * Replace a substring, allocating space to hold the new one.
 */
char *
replace_substring (char **str, const char *old, const char *new) {
    char *cp;

    if ((cp = strstr (*str, old))) {
        char *remainder = cp + strlen (old);
        char *prefix, *new_str;

        if (cp - *str) {
            prefix = mh_xstrdup(*str);
            *(prefix + (cp - *str)) = '\0';
            new_str = concat (prefix, new, remainder, NULL);
            free (prefix);
        } else {
            new_str = concat (new, remainder, NULL);
        }

        free (*str);

        return *str = new_str;
    } else {
        return NULL;
    }
}


/*
 * Remove a name=value parameter, given just its name, from a header value.
 */
char *
remove_parameter (char *str, const char *name) {
    /* It looks to me, based on the BNF in RFC 2045, than there can't
       be whitespace betwwen the parameter name and the "=", or
       between the "=" and the parameter value. */
    char *param_name = concat (name, "=", NULL);
    char *cp;

    if ((cp = strstr (str, param_name))) {
        char *start, *end;
        size_t count = 1;

        /* Remove any leading spaces, before the parameter name. */
        for (start = cp;
             start > str && isspace ((unsigned char) *(start-1));
             --start) {
            continue;
        }
        /* Remove a leading semicolon. */
        if (start > str  &&  *(start-1) == ';') { --start; }

        end = cp + strlen (name) + 1;
        if (*end == '"') {
            /* Skip past the quoted value, and then the final quote. */
            for (++end ; *end  &&  *end != '"'; ++end) { continue; }
            ++end;
        } else {
            /* Skip past the value. */
            for (++end ; *end  &&  ! isspace ((unsigned char) *end); ++end) {}
        }

        /* Count how many characters need to be moved.  Include
           trailing null, which is accounted for by the
           initialization of count to 1. */
        for (cp = end; *cp; ++cp) { ++count; }
        (void) memmove (start, end, count);
    }

    free (param_name);

    return str;
}


/*
 * Fix Content-Transfer-Encoding of composite,, e.g., message or multipart, part.
 * According to RFC 2045 Sec. 6.4, it must be 7bit, 8bit, or binary.  Set it to
 * 8 bit.
 */
static int
fix_composite_cte (CT ct, int *message_mods) {
    int status = OK;

    if (ct->c_type == CT_MESSAGE  ||  ct->c_type == CT_MULTIPART) {
        if (ct->c_encoding != CE_7BIT  &&  ct->c_encoding != CE_8BIT  &&
            ct->c_encoding != CE_BINARY) {
            HF hf;

            for (hf = ct->c_first_hf; hf; hf = hf->next) {
                char *name = hf->name;
                for (; *name && isspace ((unsigned char) *name); ++name) {
                    continue;
                }

                if (! strncasecmp (name, ENCODING_FIELD,
                                   strlen (ENCODING_FIELD))) {
                    char *prefix = "Nmh-REPLACED-INVALID-";
                    HF h;

                    NEW(h);
                    h->name = add (hf->name, NULL);
                    h->hf_encoding = hf->hf_encoding;
                    h->next = hf->next;
                    hf->next = h;

                    /* Retain old header but prefix its name. */
                    free (hf->name);
                    hf->name = concat (prefix, h->name, NULL);

                    ++*message_mods;
                    if (verbosw) {
                        char *encoding = cpytrim (hf->value);
                        report (NULL, ct->c_partno, ct->c_file,
                                "replace Content-Transfer-Encoding of %s "
                                "with 8 bit", encoding);
                        free (encoding);
                    }

                    h->value = add (" 8bit\n", NULL);

                    /* Don't need to warn for multiple C-T-E header
                       fields, parse_mime() already does that.  But
                       if there are any, fix them all as necessary. */
                    hf = h;
                }
            }

            set_ce (ct, CE_8BIT);
        }

        if (ct->c_type == CT_MULTIPART) {
            struct multipart *m;
            struct part *part;

            m = (struct multipart *) ct->c_ctparams;
            for (part = m->mp_parts; part; part = part->mp_next) {
                if (fix_composite_cte (part->mp_part, message_mods) != OK) {
                    status = NOTOK;
                    break;
                }
            }
        }
    }

    return status;
}


/*
 * Set content encoding.
 */
static int
set_ce (CT ct, int encoding) {
    const char *ce = ce_str (encoding);
    const struct str2init *ctinit = get_ce_method (ce);

    if (ctinit) {
        char *cte = concat (" ", ce, "\n", NULL);
        int found_cte = 0;
        HF hf;
        /* Decoded contents might be in ct->c_cefile.ce_file, if the
           caller is decode_text_parts ().  Save because we'll
           overwrite below. */
        struct cefile decoded_content_info = ct->c_cefile;

        ct->c_encoding = encoding;

        ct->c_ctinitfnx = ctinit->si_init;
        /* This will assign ct->c_cefile with an all-0 struct, which
           is what we want. */
        (*ctinit->si_init) (ct);
        /* After returning, the caller should set
           ct->c_cefile.ce_file to the name of the file containing
           the contents. */

        if (ct->c_ceclosefnx) {
            (*ct->c_ceclosefnx) (ct);
        }

        /* Restore the cefile. */
        ct->c_cefile = decoded_content_info;

        /* Update/add Content-Transfer-Encoding header field. */
        for (hf = ct->c_first_hf; hf; hf = hf->next) {
            if (! strcasecmp (ENCODING_FIELD, hf->name)) {
                found_cte = 1;
                free (hf->value);
                hf->value = cte;
            }
        }
        if (! found_cte) {
            add_header (ct, add (ENCODING_FIELD, NULL), cte);
        }

        /* Update c_celine.  It's used only by mhlist -debug. */
        free (ct->c_celine);
        ct->c_celine = add (cte, NULL);

        return OK;
    } else {
        return NOTOK;
    }
}


/*
 * Make sure each text part has a corresponding text/plain part.
 */
static int
ensure_text_plain (CT *ct, CT parent, int *message_mods, int replacetextplain) {
    int status = OK;

    switch ((*ct)->c_type) {
    case CT_TEXT: {
        /* Nothing to do for text/plain. */
        if ((*ct)->c_subtype == TEXT_PLAIN) { return OK; }

        if (parent  &&  parent->c_type == CT_MULTIPART  &&
            parent->c_subtype == MULTI_ALTERNATE) {
            int new_subpart_number = 1;
            int has_text_plain =
                find_textplain_sibling (parent, replacetextplain,
                                        &new_subpart_number);

            if (! has_text_plain) {
                /* Parent is a multipart/alternative.  Insert a new
                   text/plain subpart. */
                const int inserted =
                    insert_new_text_plain_part (*ct, new_subpart_number,
                                                parent);
                if (inserted) {
                    ++*message_mods;
                    if (verbosw) {
                        report (NULL, parent->c_partno, parent->c_file,
                                "insert text/plain part");
                    }
                } else {
                    status = NOTOK;
                }
            }
        } else if (parent  &&  parent->c_type == CT_MULTIPART  &&
            parent->c_subtype == MULTI_RELATED) {
            char *type_subtype =
                concat ((*ct)->c_ctinfo.ci_type, "/",
                        (*ct)->c_ctinfo.ci_subtype, NULL);
            const char *parent_type =
                get_param (parent->c_ctinfo.ci_first_pm, "type", '?', 1);
            int new_subpart_number = 1;
            int has_text_plain = 0;

            /* Have to do string comparison on the subtype because we
               don't enumerate all of them in c_subtype values.
               parent_type will be NULL if the multipart/related part
               doesn't have a type parameter.  The type parameter must
               be specified according to RFC 2387 Sec. 3.1 but not all
               messages comply. */
            if (parent_type  &&  strcasecmp (type_subtype, parent_type) == 0) {
                /* The type of this part matches the root type of the
                   parent multipart/related.  Look to see if there's
                   text/plain sibling. */
                has_text_plain =
                    find_textplain_sibling (parent, replacetextplain,
                                            &new_subpart_number);
            }

            free (type_subtype);

            if (! has_text_plain) {
                struct multipart *mp = (struct multipart *) parent->c_ctparams;
                struct part *part;
                int siblings = 0;

                for (part = mp->mp_parts; part; part = part->mp_next) {
                    if (*ct != part->mp_part) {
                        ++siblings;
                    }
                }

                if (siblings) {
                    /* Parent is a multipart/related.  Insert a new
                       text/plain subpart in a new multipart/alternative. */
                    if (insert_into_new_mp_alt (ct, message_mods)) {
                        /* Not an error if text/plain couldn't be added. */
                    }
                } else {
                    /* There are no siblings, so insert a new text/plain
                       subpart, and change the parent type from
                       multipart/related to multipart/alternative. */
                    const int inserted =
                        insert_new_text_plain_part (*ct, new_subpart_number,
                                                    parent);

                    if (inserted) {
                        HF hf;

                        parent->c_subtype = MULTI_ALTERNATE;
                        parent->c_ctinfo.ci_subtype = mh_xstrdup("alternative");
                        if (! replace_substring (&parent->c_ctline, "/related",
                                                 "/alternative")) {
                            advise (NULL,
                                    "did not find multipart/related in %s",
                                    parent->c_ctline);
                        }

                        /* Update Content-Type header field. */
                        for (hf = parent->c_first_hf; hf; hf = hf->next) {
                            if (! strcasecmp (TYPE_FIELD, hf->name)) {
                                if (replace_substring (&hf->value, "/related",
                                                       "/alternative")) {
                                    ++*message_mods;
                                    if (verbosw) {
                                        report (NULL, parent->c_partno,
                                                parent->c_file,
                                                "insert text/plain part");
                                    }

                                    /* Remove, e.g., type="text/html" from
                                       multipart/alternative. */
                                    remove_parameter (hf->value, "type");
                                    break;
                                } else {
                                    advise (NULL, "did not find multipart/"
                                                  "related in header %s",
                                            hf->value);
                                }
                            }
                        }
                    } else {
                        /* Not an error if text/plain couldn't be inserted. */
                    }
                }
            }
        } else {
            if (insert_into_new_mp_alt (ct, message_mods)) {
                status = NOTOK;
            }
        }
        break;
    }

    case CT_MULTIPART: {
        struct multipart *mp = (struct multipart *) (*ct)->c_ctparams;
        struct part *part;

        for (part = mp->mp_parts; status == OK && part; part = part->mp_next) {
            if ((*ct)->c_type == CT_MULTIPART) {
                status = ensure_text_plain (&part->mp_part, *ct, message_mods,
                                            replacetextplain);
            }
        }
        break;
    }

    case CT_MESSAGE:
        if ((*ct)->c_subtype == MESSAGE_EXTERNAL) {
            struct exbody *e = (struct exbody *) (*ct)->c_ctparams;

            status = ensure_text_plain (&e->eb_content, *ct, message_mods,
                                        replacetextplain);
        }
        break;
    }

    return status;
}


/*
 * See if there is a sibling text/plain, and return its subpart number.
 */
static int
find_textplain_sibling (CT parent, int replacetextplain,
                        int *new_subpart_number) {
    struct multipart *mp = (struct multipart *) parent->c_ctparams;
    struct part *part, *prev;
    int has_text_plain = 0;

    for (prev = part = mp->mp_parts; part; part = part->mp_next) {
        ++*new_subpart_number;
        if (part->mp_part->c_type == CT_TEXT  &&
            part->mp_part->c_subtype == TEXT_PLAIN) {
            if (replacetextplain) {
                struct part *old_part;
                if (part == mp->mp_parts) {
                    old_part = mp->mp_parts;
                    mp->mp_parts = part->mp_next;
                } else {
                    old_part = prev->mp_next;
                    prev->mp_next = part->mp_next;
                }
                if (verbosw) {
                    report (NULL, parent->c_partno, parent->c_file,
                            "remove text/plain part %s",
                            old_part->mp_part->c_partno);
                }
                free_content (old_part->mp_part);
                free (old_part);
            } else {
                has_text_plain = 1;
            }
            break;
        }
        prev = part;
    }

    return has_text_plain;
}


/*
 * Insert a new text/plain part.
 */
static int
insert_new_text_plain_part (CT ct, int new_subpart_number, CT parent) {
    struct multipart *mp = (struct multipart *) parent->c_ctparams;
    struct part *new_part;

    NEW(new_part);
    if ((new_part->mp_part = build_text_plain_part (ct))) {
        char buffer[16];
        snprintf (buffer, sizeof buffer, "%d", new_subpart_number);

        new_part->mp_next = mp->mp_parts;
        mp->mp_parts = new_part;
        new_part->mp_part->c_partno =
            concat (parent->c_partno ? parent->c_partno : "1", ".",
                    buffer, NULL);

        return 1;
    } else {
        free_content (new_part->mp_part);
        free (new_part);

        return 0;
    }
}


/*
 * Create a text/plain part to go along with non-plain sibling part.
 */
static CT
build_text_plain_part (CT encoded_part) {
    CT tp_part = divide_part (encoded_part);
    char *tmp_plain_file = NULL;

    if (decode_part (tp_part) == OK) {
        /* Now, tp_part->c_cefile.ce_file is the name of the tmp file that
           contains the decoded contents.  And the decoding function, such
           as openQuoted, will have set ...->ce_unlink to 1 so that it will
           be unlinked by free_content (). */
        char *tempfile;

        if ((tempfile = m_mktemp2 (NULL, invo_name, NULL, NULL)) == NULL) {
            advise (NULL, "unable to create temporary file in %s",
                    get_temp_dir());
        } else {
            tmp_plain_file = add (tempfile, NULL);
            if (reformat_part (tp_part, tmp_plain_file,
                               tp_part->c_ctinfo.ci_type,
                               tp_part->c_ctinfo.ci_subtype,
                               tp_part->c_type) == OK) {
                return tp_part;
            }
        }
    }

    free_content (tp_part);
    if (tmp_plain_file) { (void) m_unlink (tmp_plain_file); }
    free (tmp_plain_file);

    return NULL;
}


/*
 * Slip new text/plain part into a new multipart/alternative.
 */
static int
insert_into_new_mp_alt (CT *ct, int *message_mods) {
    CT tp_part = build_text_plain_part (*ct);
    int status = OK;

    if (tp_part) {
        CT mp_alt = build_multipart_alt (*ct, tp_part, CT_MULTIPART,
                                         MULTI_ALTERNATE);
        if (mp_alt) {
            struct multipart *mp = (struct multipart *) mp_alt->c_ctparams;

            if (mp  &&  mp->mp_parts) {
                mp->mp_parts->mp_part = tp_part;
                /* Make the new multipart/alternative the parent. */
                *ct = mp_alt;

                ++*message_mods;
                if (verbosw) {
                    report (NULL, (*ct)->c_partno, (*ct)->c_file,
                            "insert text/plain part");
                }
            } else {
                free_content (tp_part);
                free_content (mp_alt);
                status = NOTOK;
            }
        } else {
            status = NOTOK;
        }
    } else {
        /* Not an error if text/plain couldn't be built. */
    }

    return status;
}


/*
 * Clone a MIME part.
 */
static CT
divide_part (CT ct) {
    CT new_part;

    NEW0(new_part);
    /* Just copy over what is needed for decoding.  c_vrsn and
       c_celine aren't necessary. */
    new_part->c_file = add (ct->c_file, NULL);
    new_part->c_begin = ct->c_begin;
    new_part->c_end = ct->c_end;
    copy_ctinfo (&new_part->c_ctinfo, &ct->c_ctinfo);
    new_part->c_type = ct->c_type;
    new_part->c_cefile = ct->c_cefile;
    new_part->c_encoding = ct->c_encoding;
    new_part->c_ctinitfnx = ct->c_ctinitfnx;
    new_part->c_ceopenfnx = ct->c_ceopenfnx;
    new_part->c_ceclosefnx = ct->c_ceclosefnx;
    new_part->c_cesizefnx = ct->c_cesizefnx;

    /* c_ctline is used by reformat__part(), so it can preserve
       anything after the type/subtype. */
    new_part->c_ctline = add (ct->c_ctline, NULL);

    return new_part;
}


/*
 * Copy the content info from one part to another.
 */
static void
copy_ctinfo (CI dest, CI src) {
    PM s_pm, d_pm;

    dest->ci_type = src->ci_type ? add (src->ci_type, NULL) : NULL;
    dest->ci_subtype = src->ci_subtype ? add (src->ci_subtype, NULL) : NULL;

    for (s_pm = src->ci_first_pm; s_pm; s_pm = s_pm->pm_next) {
    	d_pm = add_param(&dest->ci_first_pm, &dest->ci_last_pm, s_pm->pm_name,
			 s_pm->pm_value, 0);
	if (s_pm->pm_charset) {
	    d_pm->pm_charset = mh_xstrdup(s_pm->pm_charset);
        }
	if (s_pm->pm_lang) {
	    d_pm->pm_lang = mh_xstrdup(s_pm->pm_lang);
        }
    }

    dest->ci_comment = src->ci_comment ? add (src->ci_comment, NULL) : NULL;
    dest->ci_magic = src->ci_magic ? add (src->ci_magic, NULL) : NULL;
}


/*
 * Decode content.
 */
static int
decode_part (CT ct) {
    char *tmp_decoded;
    int status;
    char *tempfile;

    if ((tempfile = m_mktemp2 (NULL, invo_name, NULL, NULL)) == NULL) {
        adios (NULL, "unable to create temporary file in %s", get_temp_dir());
    }
    tmp_decoded = add (tempfile, NULL);
    /* The following call will load ct->c_cefile.ce_file with the tmp
       filename of the decoded content.  tmp_decoded will contain the
       encoded output, get rid of that. */
    status = output_message (ct, tmp_decoded);
    (void) m_unlink (tmp_decoded);
    free (tmp_decoded);

    return status;
}


/*
 * Reformat content as plain text.
 * Some of the arguments aren't really needed now, but maybe will
 * be in the future for other than text types.
 */
static int
reformat_part (CT ct, char *file, char *type, char *subtype, int c_type) {
    int output_subtype, output_encoding;
    const char *reason = NULL;
    char *cp, *cf;
    int status;

    /* Hacky:  this redirects the output from whatever command is used
       to show the part to a file.  So, the user can't have any output
       redirection in that command.
       Could show_multi() in mhshowsbr.c avoid this? */

    /* Check for invo_name-format-type/subtype. */
    if ((cf = context_find_by_type ("format", type, subtype)) == NULL) {
        if (verbosw) {
            advise (NULL, "Don't know how to convert %s, there is no "
                    "%s-format-%s/%s profile entry",
                    ct->c_file, invo_name, type, subtype);
        }
        return NOTOK;
    } else {
        if (strchr (cf, '>')) {
            advise (NULL, "'>' prohibited in \"%s\",\nplease fix your "
                    "%s-format-%s/%s profile entry", cf, invo_name, type,
                    subtype ? subtype : "");

            return NOTOK;
        }
    }

    cp = concat (cf, " >", file, NULL);
    status = show_content_aux (ct, 0, cp, NULL, NULL);
    free (cp);

    /* Unlink decoded content tmp file and free its filename to avoid
       leaks.  The file stream should already have been closed. */
    if (ct->c_cefile.ce_unlink) {
        (void) m_unlink (ct->c_cefile.ce_file);
        free (ct->c_cefile.ce_file);
        ct->c_cefile.ce_file = NULL;
        ct->c_cefile.ce_unlink = 0;
    }

    if (c_type == CT_TEXT) {
        output_subtype = TEXT_PLAIN;
    } else {
        /* Set subtype to 0, which is always an UNKNOWN subtype. */
        output_subtype = 0;
    }

    output_encoding = content_encoding (ct, &reason);
    if (set_ct_type (ct, c_type, output_subtype, output_encoding) == OK) {
        ct->c_cefile.ce_file = file;
        ct->c_cefile.ce_unlink = 1;
    } else {
        ct->c_cefile.ce_unlink = 0;
        status = NOTOK;
    }

    return status;
}


/*
 * Fill in a multipart/alternative part.
 */
static CT
build_multipart_alt (CT first_alt, CT new_part, int type, int subtype) {
    char *boundary_prefix = "----=_nmh-multipart";
    char *boundary = concat (boundary_prefix, first_alt->c_partno, NULL);
    char *boundary_indicator = "; boundary=";
    char *typename, *subtypename, *name;
    CT ct;
    struct part *p;
    struct multipart *m;
    const struct str2init *ctinit;

    NEW0(ct);

    /* Set up the multipart/alternative part.  These fields of *ct were
       initialized to 0 by mh_xcalloc():
       c_fp, c_unlink, c_begin, c_end,
       c_vrsn, c_ctline, c_celine,
       c_id, c_descr, c_dispo, c_partno,
       c_ctinfo.ci_comment, c_ctinfo.ci_magic,
       c_cefile, c_encoding,
       c_digested, c_digest[16], c_ctexbody,
       c_ctinitfnx, c_ceopenfnx, c_ceclosefnx, c_cesizefnx,
       c_umask, c_rfc934,
       c_showproc, c_termproc, c_storeproc, c_storage, c_folder
    */

    ct->c_file = add (first_alt->c_file, NULL);
    ct->c_type = type;
    ct->c_subtype = subtype;

    ctinit = get_ct_init (ct->c_type);

    typename = ct_type_str (type);
    subtypename = ct_subtype_str (type, subtype);

    {
        int serial = 0;
        int found_boundary = 1;

        while (found_boundary  &&  serial < 1000000) {
            found_boundary = 0;

            /* Ensure that the boundary doesn't appear in the decoded
               content. */
            if (new_part->c_cefile.ce_file) {
                if ((found_boundary =
                     boundary_in_content (&new_part->c_cefile.ce_fp,
                                          new_part->c_cefile.ce_file,
                                          boundary)) == -1) {
                    free (ct);
                    return NULL;
                }
            }

            /* Ensure that the boundary doesn't appear in the encoded
               content. */
            if (! found_boundary  &&  new_part->c_file) {
                if ((found_boundary = boundary_in_content (&new_part->c_fp,
                                                           new_part->c_file,
                                                           boundary)) == -1) {
                    free (ct);
                    return NULL;
                }
            }

            if (found_boundary) {
                /* Try a slightly different boundary. */
                char buffer2[16];

                free (boundary);
                ++serial;
                snprintf (buffer2, sizeof buffer2, "%d", serial);
                boundary =
                    concat (boundary_prefix,
                            first_alt->c_partno ? first_alt->c_partno : "",
                            "-", buffer2,  NULL);
            }
        }

        if (found_boundary) {
            advise (NULL, "giving up trying to find a unique boundary");
            free (ct);
            return NULL;
        }
    }

    name = concat (" ", typename, "/", subtypename, boundary_indicator, "\"",
                   boundary, "\"", NULL);

    /* Load c_first_hf and c_last_hf. */
    transfer_noncontent_headers (first_alt, ct);
    add_header (ct, add (TYPE_FIELD, NULL), concat (name, "\n", NULL));
    free (name);

    /* Load c_partno. */
    if (first_alt->c_partno) {
        ct->c_partno = add (first_alt->c_partno, NULL);
        free (first_alt->c_partno);
        first_alt->c_partno = concat (ct->c_partno, ".1", NULL);
        new_part->c_partno = concat (ct->c_partno, ".2", NULL);
    } else {
        first_alt->c_partno = add ("1", NULL);
        new_part->c_partno = add ("2", NULL);
    }

    if (ctinit) {
        ct->c_ctinfo.ci_type = add (typename, NULL);
        ct->c_ctinfo.ci_subtype = add (subtypename, NULL);
    }

    add_param(&ct->c_ctinfo.ci_first_pm, &ct->c_ctinfo.ci_last_pm,
              "boundary", boundary, 0);

    NEW(p);
    NEW(p->mp_next);
    p->mp_next->mp_next = NULL;
    p->mp_next->mp_part = first_alt;

    NEW0(m);
    m->mp_start = concat (boundary, "\n", NULL);
    m->mp_stop = concat (boundary, "--\n", NULL);
    m->mp_parts = p;
    ct->c_ctparams = m;

    free (boundary);

    return ct;
}


/*
 * Check that the boundary does not appear in the content.
 */
static int
boundary_in_content (FILE **fp, char *file, const char *boundary) {
    char buffer[BUFSIZ];
    size_t bytes_read;
    int found_boundary = 0;

    /* free_content() will close *fp if we fopen it here. */
    if (! *fp  &&  (*fp = fopen (file, "r")) == NULL) {
        advise (file, "unable to open %s for reading", file);
        return NOTOK;
    }

    fseeko (*fp, 0L, SEEK_SET);
    while ((bytes_read = fread (buffer, 1, sizeof buffer, *fp)) > 0) {
        if (find_str (buffer, bytes_read, boundary)) {
            found_boundary = 1;
            break;
        }
    }

    return found_boundary;
}


/*
 * Remove all non-Content headers.
 */
static void
transfer_noncontent_headers (CT old, CT new) {
    HF hp, hp_prev;

    hp_prev = hp = old->c_first_hf;
    while (hp) {
        HF next = hp->next;

        if (strncasecmp (XXX_FIELD_PRF, hp->name, strlen (XXX_FIELD_PRF))) {
            if (hp == old->c_last_hf) {
                if (hp == old->c_first_hf) {
                    old->c_last_hf =  old->c_first_hf = NULL;
                } else {
                    hp_prev->next = NULL;
                    old->c_last_hf =  hp_prev;
                }
            } else {
                if (hp == old->c_first_hf) {
                    old->c_first_hf = next;
                } else {
                    hp_prev->next = next;
                }
            }

            /* Put node hp in the new CT. */
            if (new->c_first_hf == NULL) {
                new->c_first_hf = hp;
            } else {
                new->c_last_hf->next = hp;
            }
            new->c_last_hf = hp;
        } else {
            /* A Content- header, leave in old. */
            hp_prev = hp;
        }

        hp = next;
    }
}


/*
 * Set content type.
 */
static int
set_ct_type (CT ct, int type, int subtype, int encoding) {
    char *typename = ct_type_str (type);
    char *subtypename = ct_subtype_str (type, subtype);
    /* E.g, " text/plain" */
    char *type_subtypename = concat (" ", typename, "/", subtypename, NULL);
    /* E.g, " text/plain\n" */
    char *name_plus_nl = concat (type_subtypename, "\n", NULL);
    int found_content_type = 0;
    HF hf;
    const char *cp = NULL;
    char *ctline;
    int status;

    /* Update/add Content-Type header field. */
    for (hf = ct->c_first_hf; hf; hf = hf->next) {
        if (! strcasecmp (TYPE_FIELD, hf->name)) {
            found_content_type = 1;
            free (hf->value);
            hf->value = (cp = strchr (ct->c_ctline, ';'))
                ?  concat (type_subtypename, cp, "\n", NULL)
                :  add (name_plus_nl, NULL);
        }
    }
    if (! found_content_type) {
        add_header (ct, add (TYPE_FIELD, NULL),
                    (cp = strchr (ct->c_ctline, ';'))
                    ?  concat (type_subtypename, cp, "\n", NULL)
                    :  add (name_plus_nl, NULL));
    }

    /* Some of these might not be used, but set them anyway. */
    ctline = cp
        ?  concat (type_subtypename, cp, NULL)
        :  concat (type_subtypename, NULL);
    free (ct->c_ctline);
    ct->c_ctline = ctline;
    /* Leave other ctinfo members as they were. */
    free (ct->c_ctinfo.ci_type);
    ct->c_ctinfo.ci_type = add (typename, NULL);
    free (ct->c_ctinfo.ci_subtype);
    ct->c_ctinfo.ci_subtype = add (subtypename, NULL);
    ct->c_type = type;
    ct->c_subtype = subtype;

    free (name_plus_nl);
    free (type_subtypename);

    status = set_ce (ct, encoding);

    return status;
}


/*
 * It's not necessary to update the charset parameter of a Content-Type
 * header for a text part.  According to RFC 2045 Sec. 6.4, the body
 * (content) was originally in the specified charset, "and will be in
 * that character set again after decoding."
 */
static int
decode_text_parts (CT ct, int encoding, const char *decodetypes,
                   int *message_mods) {
    int status = OK;
    int lf_line_endings = 0;

    switch (ct->c_type) {
    case CT_MULTIPART: {
        struct multipart *m = (struct multipart *) ct->c_ctparams;
        struct part *part;

        /* Should check to see if the body for this part is encoded?
           For now, it gets passed along as-is by InitMultiPart(). */
        for (part = m->mp_parts; status == OK  &&  part; part = part->mp_next) {
            status = decode_text_parts (part->mp_part, encoding, decodetypes,
                                        message_mods);
        }
        break;
    }

    case CT_MESSAGE:
        if (ct->c_subtype == MESSAGE_EXTERNAL) {
            struct exbody *e = (struct exbody *) ct->c_ctparams;

            status = decode_text_parts (e->eb_content, encoding, decodetypes,
                                        message_mods);
        }
        break;

    default:
        if (! should_decode(decodetypes, ct->c_ctinfo.ci_type, ct->c_ctinfo.ci_subtype)) {
            break;
        }

        lf_line_endings =
            ct->c_ctparams  &&  ((struct text *) ct->c_ctparams)->lf_line_endings;

        switch (ct->c_encoding) {
        case CE_BASE64:
        case CE_QUOTED: {
            int ct_encoding;

            if (decode_part (ct) == OK  &&  ct->c_cefile.ce_file) {
                const char *reason = NULL;

                if ((ct_encoding = content_encoding (ct, &reason)) == CE_BINARY
                    &&  encoding != CE_BINARY) {
                    /* The decoding isn't acceptable so discard it.
                       Leave status as OK to allow other transformations. */
                    if (verbosw) {
                        report (NULL, ct->c_partno, ct->c_file,
                                "will not decode%s because it is binary (%s)",
                                ct->c_partno  ?  ""
                                              :  ct->c_ctline  ?  ct->c_ctline
                                                               :  "",
                                reason);
                    }
                    (void) m_unlink (ct->c_cefile.ce_file);
                    free (ct->c_cefile.ce_file);
                    ct->c_cefile.ce_file = NULL;
                } else if (ct->c_encoding == CE_QUOTED  &&
                           ct_encoding == CE_8BIT  &&  encoding == CE_7BIT) {
                    /* The decoding isn't acceptable so discard it.
                       Leave status as OK to allow other transformations. */
                    if (verbosw) {
                        report (NULL, ct->c_partno, ct->c_file,
                                "will not decode%s because it is 8bit",
                                ct->c_partno  ?  ""
                                              :  ct->c_ctline  ?  ct->c_ctline
                                                               :  "");
                    }
                    (void) m_unlink (ct->c_cefile.ce_file);
                    free (ct->c_cefile.ce_file);
                    ct->c_cefile.ce_file = NULL;
                } else {
                    int enc;

                    if (ct_encoding == CE_BINARY) {
                        enc = CE_BINARY;
                    } else if (ct_encoding == CE_8BIT  &&  encoding == CE_7BIT) {
                        enc = CE_QUOTED;
                    } else {
                        enc = ct_encoding;
                    }
                    if (set_ce (ct, enc) == OK) {
                        ++*message_mods;
                        if (verbosw) {
                            report (NULL, ct->c_partno, ct->c_file, "decode%s",
                                    ct->c_ctline ? ct->c_ctline : "");
                        }
                        if (lf_line_endings) {
                            strip_crs (ct, message_mods);
                        }
                    } else {
                        status = NOTOK;
                    }
                }
            } else {
                status = NOTOK;
            }
            break;
        }
        case CE_8BIT:
        case CE_7BIT:
            if (lf_line_endings) {
                strip_crs (ct, message_mods);
            }
            break;
        default:
            break;
        }

        break;
    }

    return status;
}


/*
 * Determine if the part with type[/subtype] should be decoded, according to
 * decodetypes (which came from the -decodetypes switch).
 */
static int
should_decode(const char *decodetypes, const char *type, const char *subtype) {
    /* Quick search for matching type[/subtype] in decodetypes:  bracket
       decodetypes with commas, then search for ,type, and ,type/subtype, in
       it. */

    int found_match = 0;
    char *delimited_decodetypes = concat(",", decodetypes, ",", NULL);
    char *delimited_type = concat(",", type, ",", NULL);

    if (nmh_strcasestr(delimited_decodetypes, delimited_type)) {
        found_match = 1;
    } else if (subtype != NULL) {
        char *delimited_type_subtype =
            concat(",", type, "/", subtype, ",", NULL);

        if (nmh_strcasestr(delimited_decodetypes, delimited_type_subtype)) {
            found_match = 1;
        }
        free(delimited_type_subtype);
    }

    free(delimited_type);
    free(delimited_decodetypes);

    return found_match;
}


/*
 * See if the decoded content is 7bit, 8bit, or binary.  It's binary
 * if it has any NUL characters, a CR not followed by a LF, or lines
 * greater than 998 characters in length.  If binary, reason is set
 *  to a string explaining why.
 */
static int
content_encoding (CT ct, const char **reason) {
    CE ce = &ct->c_cefile;
    int encoding = CE_7BIT;

    if (ce->ce_file) {
        size_t line_len = 0;
        char buffer[BUFSIZ];
        size_t inbytes;

        if (! ce->ce_fp  &&  (ce->ce_fp = fopen (ce->ce_file, "r")) == NULL) {
            advise (ce->ce_file, "unable to open for reading");
            return CE_UNKNOWN;
        }

        fseeko (ce->ce_fp, 0L, SEEK_SET);
        while (encoding != CE_BINARY  &&
               (inbytes = fread (buffer, 1, sizeof buffer, ce->ce_fp)) > 0) {
            char *cp;
            size_t i;
            int last_char_was_cr = 0;

            for (i = 0, cp = buffer; i < inbytes; ++i, ++cp) {
                if (*cp == '\0'  ||  ++line_len > 998  ||
                    (*cp != '\n'  &&  last_char_was_cr)) {
                    encoding = CE_BINARY;
                    if (*cp == '\0') {
                        *reason = "null character";
                    } else if (line_len > 998) {
                        *reason = "line length > 998";
                    } else if (*cp != '\n'  &&  last_char_was_cr) {
                        *reason = "CR not followed by LF";
                    } else {
                        /* Should not reach this. */
                        *reason = "";
                    }
                    break;
                } else if (*cp == '\n') {
                    line_len = 0;
                } else if (! isascii ((unsigned char) *cp)) {
                    encoding = CE_8BIT;
                }

                last_char_was_cr = *cp == '\r'  ?  1  :  0;
            }
        }

        fclose (ce->ce_fp);
        ce->ce_fp = NULL;
    } /* else should never happen */

    return encoding;
}


/*
 * Strip carriage returns from content.
 */
static int
strip_crs (CT ct, int *message_mods) {
    char *charset = content_charset (ct);
    int status = OK;

    /* Only strip carriage returns if content is ASCII or another
       charset that has the same readily recognizable CR followed by a
       LF.  We can include UTF-8 here because if the high-order bit of
       a UTF-8 byte is 0, then it must be a single-byte ASCII
       character. */
    if (! strcasecmp (charset, "US-ASCII")  ||
        ! strcasecmp (charset, "UTF-8")  ||
        ! strncasecmp (charset, "ISO-8859-", 9)  ||
        ! strncasecmp (charset, "WINDOWS-12", 10)) {
        char **file = NULL;
        FILE **fp = NULL;
        size_t begin;
        size_t end;
        int has_crs = 0;
        int opened_input_file = 0;

        if (ct->c_cefile.ce_file) {
            file = &ct->c_cefile.ce_file;
            fp = &ct->c_cefile.ce_fp;
            begin = end = 0;
        } else if (ct->c_file) {
            file = &ct->c_file;
            fp = &ct->c_fp;
            begin = (size_t) ct->c_begin;
            end = (size_t) ct->c_end;
        } /* else don't know where the content is */

        if (file  &&  *file  &&  fp) {
            if (! *fp) {
                if ((*fp = fopen (*file, "r")) == NULL) {
                    advise (*file, "unable to open for reading");
                    status = NOTOK;
                } else {
                    opened_input_file = 1;
                }
            }
        }

        if (fp  &&  *fp) {
            char buffer[BUFSIZ];
            size_t bytes_read;
            size_t bytes_to_read =
                end > 0 && end > begin  ?  end - begin  :  sizeof buffer;

            fseeko (*fp, begin, SEEK_SET);
            while ((bytes_read = fread (buffer, 1,
                                        min (bytes_to_read, sizeof buffer),
                                        *fp)) > 0) {
                /* Look for CR followed by a LF.  This is supposed to
                   be text so there should be LF's.  If not, don't
                   modify the content. */
                char *cp;
                size_t i;
                int last_char_was_cr = 0;

                if (end > 0) { bytes_to_read -= bytes_read; }

                for (i = 0, cp = buffer; i < bytes_read; ++i, ++cp) {
                    if (*cp == '\n'  &&  last_char_was_cr) {
                        has_crs = 1;
                        break;
                    }

                    last_char_was_cr = *cp == '\r'  ?  1  :  0;
                }
            }

            if (has_crs) {
                int fd;
                char *stripped_content_file;
                char *tempfile = m_mktemp2 (NULL, invo_name, &fd, NULL);

                if (tempfile == NULL) {
                    adios (NULL, "unable to create temporary file in %s",
                           get_temp_dir());
                }
                stripped_content_file = add (tempfile, NULL);

                /* Strip each CR before a LF from the content. */
                fseeko (*fp, begin, SEEK_SET);
                while ((bytes_read = fread (buffer, 1, sizeof buffer, *fp)) >
                       0) {
                    char *cp;
                    size_t i;
                    int last_char_was_cr = 0;

                    for (i = 0, cp = buffer; i < bytes_read; ++i, ++cp) {
                        if (*cp == '\r') {
                            last_char_was_cr = 1;
                        } else if (last_char_was_cr) {
                            if (*cp != '\n') {
                                if (write (fd, "\r", 1) < 0) {
                                    advise (tempfile, "CR write");
                                }
                            }
                            if (write (fd, cp, 1) < 0) {
                                advise (tempfile, "write");
                            }
                            last_char_was_cr = 0;
                        } else {
                            if (write (fd, cp, 1) < 0) {
                                advise (tempfile, "write");
                            }
                            last_char_was_cr = 0;
                        }
                    }
                }

                if (close (fd)) {
                    admonish (NULL, "unable to write temporary file %s",
                              stripped_content_file);
                    (void) m_unlink (stripped_content_file);
                    status = NOTOK;
                } else {
                    /* Replace the decoded file with the converted one. */
                    if (ct->c_cefile.ce_file) {
                        if (ct->c_cefile.ce_unlink) {
                            (void) m_unlink (ct->c_cefile.ce_file);
                        }
                        free (ct->c_cefile.ce_file);
                    }
                    ct->c_cefile.ce_file = stripped_content_file;
                    ct->c_cefile.ce_unlink = 1;

                    ++*message_mods;
                    if (verbosw) {
                        report (NULL, ct->c_partno,
                                begin == 0 && end == 0  ?  ""  :  *file,
                                "stripped CRs");
                    }
                }
            }

            if (opened_input_file) {
                fclose (*fp);
                *fp = NULL;
            }
        }
    }

    free (charset);

    return status;
}


/*
 * Add/update, if necessary, the message C-T-E, based on the least restrictive
 * of the part C-T-E's.
 */
static void
update_cte (CT ct) {
    const int least_restrictive_enc = least_restrictive_encoding (ct);

    if (least_restrictive_enc != CE_UNKNOWN  &&
        least_restrictive_enc != CE_7BIT) {
        char *cte = concat (" ", ce_str (least_restrictive_enc), "\n", NULL);
        HF hf;
        int found_cte = 0;

        /* Update/add Content-Transfer-Encoding header field. */
        for (hf = ct->c_first_hf; hf; hf = hf->next) {
            if (! strcasecmp (ENCODING_FIELD, hf->name)) {
                found_cte = 1;
                free (hf->value);
                hf->value = cte;
            }
        }
        if (! found_cte) {
            add_header (ct, add (ENCODING_FIELD, NULL), cte);
        }
    }
}


/*
 * Find the least restrictive encoding (7bit, 8bit, binary) of the parts
 * within a message.
 */
static int
least_restrictive_encoding (CT ct) {
    int encoding = CE_UNKNOWN;

    switch (ct->c_type) {
    case CT_MULTIPART: {
        struct multipart *m = (struct multipart *) ct->c_ctparams;
        struct part *part;

        for (part = m->mp_parts; part; part = part->mp_next) {
            const int part_encoding =
                least_restrictive_encoding (part->mp_part);

            if (less_restrictive (encoding, part_encoding)) {
                encoding = part_encoding;
            }
        }
        break;
    }

    case CT_MESSAGE:
        if (ct->c_subtype == MESSAGE_EXTERNAL) {
            struct exbody *e = (struct exbody *) ct->c_ctparams;
            const int part_encoding =
                least_restrictive_encoding (e->eb_content);

            if (less_restrictive (encoding, part_encoding)) {
                encoding = part_encoding;
            }
        }
        break;

    default: {
        if (less_restrictive (encoding, ct->c_encoding)) {
            encoding = ct->c_encoding;
        }
    }}

    return encoding;
}


/*
 * Return whether the second encoding is less restrictive than the first, where
 * "less restrictive" is in the sense used by RFC 2045 Secs. 6.1 and 6.4.  So,
 *   CE_BINARY is less restrictive than CE_8BIT and
 *   CE_8BIT is less restrictive than CE_7BIT.
 */
static int
less_restrictive (int encoding, int second_encoding) {
    switch (second_encoding) {
    case CE_BINARY:
        return encoding != CE_BINARY;
    case CE_8BIT:
        return encoding != CE_BINARY  &&  encoding != CE_8BIT;
    case CE_7BIT:
        return encoding != CE_BINARY  &&  encoding != CE_8BIT  &&
            encoding != CE_7BIT;
    default :
        return 0;
    }
}


/*
 * Convert character set of each part.
 */
static int
convert_charsets (CT ct, char *dest_charset, int *message_mods) {
    int status = OK;

    switch (ct->c_type) {
    case CT_TEXT:
        if (ct->c_subtype == TEXT_PLAIN) {
            status = convert_charset (ct, dest_charset, message_mods);
            if (status == OK) {
                if (verbosw) {
                    char *ct_charset = content_charset (ct);

                    report (NULL, ct->c_partno, ct->c_file,
                            "convert %s to %s", ct_charset, dest_charset);
                    free (ct_charset);
                }
            } else {
                char *ct_charset = content_charset (ct);

                report ("iconv", ct->c_partno, ct->c_file,
                        "failed to convert %s to %s", ct_charset, dest_charset);
                free (ct_charset);
            }
        }
        break;

    case CT_MULTIPART: {
        struct multipart *m = (struct multipart *) ct->c_ctparams;
        struct part *part;

        /* Should check to see if the body for this part is encoded?
           For now, it gets passed along as-is by InitMultiPart(). */
        for (part = m->mp_parts; status == OK  &&  part; part = part->mp_next) {
            status =
                convert_charsets (part->mp_part, dest_charset, message_mods);
        }
        break;
    }

    case CT_MESSAGE:
        if (ct->c_subtype == MESSAGE_EXTERNAL) {
            struct exbody *e = (struct exbody *) ct->c_ctparams;

            status =
                convert_charsets (e->eb_content, dest_charset, message_mods);
        }
        break;

    default:
        break;
    }

    return status;
}


/*
 * Fix various problems that aren't handled elsewhere.  These
 * are fixed unconditionally:  there are no switches to disable
 * them.  Currently, "problems" are these:
 * 1) remove extraneous semicolon at the end of a header parameter list
 * 2) replace RFC 2047 encoding with RFC 2231 encoding of name and
 *    filename parameters in Content-Type and Content-Disposition
 *    headers, respectively.
 */
static int
fix_always (CT ct, int *message_mods) {
    int status = OK;

    switch (ct->c_type) {
    case CT_MULTIPART: {
        struct multipart *m = (struct multipart *) ct->c_ctparams;
        struct part *part;

        for (part = m->mp_parts; status == OK  &&  part; part = part->mp_next) {
            status = fix_always (part->mp_part, message_mods);
        }
        break;
    }

    case CT_MESSAGE:
        if (ct->c_subtype == MESSAGE_EXTERNAL) {
            struct exbody *e = (struct exbody *) ct->c_ctparams;

            status = fix_always (e->eb_content, message_mods);
        }
        break;

    default: {
        HF hf;

        if (ct->c_first_hf) {
            fix_filename_encoding (ct);
        }

        for (hf = ct->c_first_hf; hf; hf = hf->next) {
            size_t len = strlen (hf->value);

            if (strcasecmp (hf->name, TYPE_FIELD) != 0  &&
                strcasecmp (hf->name, DISPO_FIELD) != 0) {
                /* Only do this for Content-Type and
                   Content-Disposition fields because those are the
                   only headers that parse_mime() warns about. */
                continue;
            }

            /* whitespace following a trailing ';' will be nuked as well */
            if (hf->value[len - 1] == '\n') {
                while (isspace((unsigned char)(hf->value[len - 2]))) {
                    if (len-- == 0) { break; }
                }
            }

            if (hf->value[len - 2] == ';') {
                /* Remove trailing ';' from parameter value. */
                hf->value[len - 2] = '\n';
                hf->value[len - 1] = '\0';

                /* Also, if Content-Type parameter, remove trailing ';'
                   from ct->c_ctline.  This probably isn't necessary
                   but can't hurt. */
                if (strcasecmp(hf->name, TYPE_FIELD) == 0 && ct->c_ctline) {
                    size_t l = strlen(ct->c_ctline) - 1;
                    while (isspace((unsigned char)(ct->c_ctline[l])) ||
                           ct->c_ctline[l] == ';') {
                        ct->c_ctline[l--] = '\0';
                        if (l == 0) { break; }
                    }
                }

                ++*message_mods;
                if (verbosw) {
                    report (NULL, ct->c_partno, ct->c_file,
                            "remove trailing ; from %s parameter value",
                            hf->name);
                }
            }
        }
    }}

    return status;
}


/*
 * Factor out common code for loops in fix_filename_encoding().
 */
static int
fix_filename_param (char *name, char *value, PM *first_pm, PM *last_pm) {
    size_t value_len;
    int fixed = 0;

    if (((value_len = strlen (value)) > 0)  &&
        strncmp (value, "=?", 2) == 0  &&
        strncmp (&value[value_len - 2], "?=", 2) == 0) {
        /* Looks like an RFC 2047 encoded parameter. */
        char decoded[PATH_MAX + 1];

        if (decode_rfc2047 (value, decoded, sizeof decoded)) {
            /* Encode using RFC 2231. */
            replace_param (first_pm, last_pm, name, decoded, 0);
            fixed = 1;
        } else {
            advise (NULL, "failed to decode %s parameter %s", name, value);
        }
    }

    return fixed;
}


/*
 * Replace RFC 2047 encoding with RFC 2231 encoding of name and
 * filename parameters in Content-Type and Content-Disposition
 * headers, respectively.
 */
static int
fix_filename_encoding (CT ct) {
    PM pm;
    HF hf;
    int fixed = 0;

    for (pm = ct->c_ctinfo.ci_first_pm; pm; pm = pm->pm_next) {
        if (pm->pm_name  &&  pm->pm_value  &&
            strcasecmp (pm->pm_name, "name") == 0) {
            fixed = fix_filename_param (pm->pm_name, pm->pm_value,
                                        &ct->c_ctinfo.ci_first_pm,
                                        &ct->c_ctinfo.ci_last_pm);
        }
    }

    for (pm = ct->c_dispo_first; pm; pm = pm->pm_next) {
        if (pm->pm_name  &&  pm->pm_value  &&
            strcasecmp (pm->pm_name, "filename") == 0) {
            fixed = fix_filename_param (pm->pm_name, pm->pm_value,
                                        &ct->c_dispo_first,
                                        &ct->c_dispo_last);
        }
    }

    /* Fix hf values to correspond. */
    for (hf = ct->c_first_hf; fixed && hf; hf = hf->next) {
        enum { OTHER, TYPE_HEADER, DISPO_HEADER } field = OTHER;

        if (strcasecmp (hf->name, TYPE_FIELD) == 0) {
            field = TYPE_HEADER;
        } else if (strcasecmp (hf->name, DISPO_FIELD) == 0) {
            field = DISPO_HEADER;
        }

        if (field != OTHER) {
            const char *const semicolon_loc = strchr (hf->value, ';');

            if (semicolon_loc) {
                const size_t len =
                    strlen (hf->name) + 1 + semicolon_loc - hf->value;
                const char *const params =
                    output_params (len,
                                   field == TYPE_HEADER
                                   ? ct->c_ctinfo.ci_first_pm
                                   : ct->c_dispo_first,
                                   NULL, 0);
                const char *const new_params = concat (params, "\n", NULL);

                replace_substring (&hf->value, semicolon_loc, new_params);
                free ((char *) new_params);
                free ((char *) params);
            } else {
                advise (NULL, "did not find semicolon in %s:%s\n",
                        hf->name, hf->value);
            }
        }
    }

    return OK;
}


/*
 * Output content in input file to output file.
 */
static int
write_content (CT ct, const char *input_filename, char *outfile, int modify_inplace,
               int message_mods) {
    int status = OK;

    if (modify_inplace) {
        if (message_mods > 0) {
            if ((status = output_message (ct, outfile)) == OK) {
                char *infile = input_filename
                    ?  add (input_filename, NULL)
                    :  add (ct->c_file ? ct->c_file : "-", NULL);

                if (remove_file (infile) == OK) {
                    if (rename (outfile, infile)) {
                        /* Rename didn't work, possibly because of an
                           attempt to rename across filesystems.  Try
                           brute force copy. */
                        int old = open (outfile, O_RDONLY);
                        int new =
                            open (infile, O_WRONLY | O_CREAT, m_gmprot ());
                        int i = -1;

                        if (old != -1  &&  new != -1) {
                            char buffer[BUFSIZ];

                            while ((i = read (old, buffer, sizeof buffer)) >
                                   0) {
                                if (write (new, buffer, i) != i) {
                                    i = -1;
                                    break;
                                }
                            }
                        }
                        if (new != -1) { close (new); }
                        if (old != -1) { close (old); }
                        (void) m_unlink (outfile);

                        if (i < 0) {
                            /* The -file argument processing used path() to
                               expand filename to absolute path. */
                            int file = ct->c_file  &&  ct->c_file[0] == '/';

                            admonish (NULL, "unable to rename %s %s to %s",
                                      file ? "file" : "message", outfile,
                                      infile);
                            status = NOTOK;
                        }
                    }
                } else {
                    admonish (NULL, "unable to remove input file %s, "
                              "not modifying it", infile);
                    (void) m_unlink (outfile);
                    status = NOTOK;
                }

                free (infile);
            } else {
                status = NOTOK;
            }
        } else {
            /* No modifications and didn't need the tmp outfile. */
            (void) m_unlink (outfile);
        }
    } else {
        /* Output is going to some file.  Produce it whether or not
           there were modifications. */
        status = output_message (ct, outfile);
    }

    flush_errors ();
    return status;
}


/*
 * parse_mime() does not set lf_line_endings in struct text, so use this
 * function to do it.  It touches the parts the decodetypes identifies.
 */
static void
set_text_ctparams(CT ct, char *decodetypes, int lf_line_endings) {
    switch (ct->c_type) {
    case CT_MULTIPART: {
        struct multipart *m = (struct multipart *) ct->c_ctparams;
        struct part *part;

        for (part = m->mp_parts; part; part = part->mp_next) {
            set_text_ctparams(part->mp_part, decodetypes, lf_line_endings);
        }
        break;
    }

    case CT_MESSAGE:
        if (ct->c_subtype == MESSAGE_EXTERNAL) {
            struct exbody *e = (struct exbody *) ct->c_ctparams;

            set_text_ctparams(e->eb_content, decodetypes, lf_line_endings);
        }
        break;

    default:
        if (should_decode(decodetypes, ct->c_ctinfo.ci_type, ct->c_ctinfo.ci_subtype)) {
            if (ct->c_ctparams == NULL) {
                ct->c_ctparams = mh_xcalloc(1, sizeof (struct text));
            }
            ((struct text *) ct->c_ctparams)->lf_line_endings = lf_line_endings;
        }
    }
}


/*
 * If "rmmproc" is defined, call that to remove the file.  Otherwise,
 * use the standard MH backup file.
 */
static int
remove_file (const char *file) {
    if (rmmproc) {
        char *rmm_command = concat (rmmproc, " ", file, NULL);
        int status = system (rmm_command);

        free (rmm_command);
        return WIFEXITED (status)  ?  WEXITSTATUS (status)  :  NOTOK;
    } else {
        /* This is OK for a non-message file, it still uses the
           BACKUP_PREFIX form.  The backup file will be in the same
           directory as file. */
        return rename (file, m_backup (file));
    }
}


/*
 * Output formatted message to user.
 */
static void
report (char *what, char *partno, char *filename, char *message, ...) {
    va_list args;
    char *fmt;

    if (verbosw) {
        va_start (args, message);
        fmt = concat (filename, partno ? " part " : ", ",
                      partno ? partno : "", partno ? ", " : "", message, NULL);

        advertise (what, NULL, fmt, args);

        free (fmt);
        va_end (args);
    }
}


static void
pipeser (int i)
{
    if (i == SIGQUIT) {
        fflush (stdout);
        fprintf (stderr, "\n");
        fflush (stderr);
    }

    done (1);
    /* NOTREACHED */
}
