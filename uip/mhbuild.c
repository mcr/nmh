/* mhbuild.c -- expand/translate MIME composition files
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/getarguments.h"
#include "sbr/smatch.h"
#include "sbr/m_backup.h"
#include "sbr/context_find.h"
#include "sbr/readconfig.h"
#include "sbr/ambigsw.h"
#include "sbr/path.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include <fcntl.h>
#include "h/mts.h"
#include "h/tws.h"
#include "h/mime.h"
#include "h/mhparse.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"
#include "sbr/m_mktemp.h"
#include "mhfree.h"
#include "mhoutsbr.h"

#define MHBUILD_SWITCHES \
    X("auto", 0, AUTOSW) \
    X("noauto", 0, NAUTOSW) \
    X("check", -5, CHECKSW) \
    X("nocheck", -7, NCHECKSW) \
    X("directives", 0, DIRECTIVES) \
    X("nodirectives", 0, NDIRECTIVES) \
    X("headers", 0, HEADSW) \
    X("noheaders", 0, NHEADSW) \
    X("list", 0, LISTSW) \
    X("nolist", 0, NLISTSW) \
    X("realsize", 0, SIZESW) \
    X("norealsize", 0, NSIZESW) \
    X("rfc934mode", 0, RFC934SW) \
    X("norfc934mode", 0, NRFC934SW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("disposition", 0, DISPOSW) \
    X("nodisposition", 0, NDISPOSW) \
    X("contentid", 0, CONTENTIDSW) \
    X("nocontentid", 0, NCONTENTIDSW) \
    X("headerencoding encoding-algorithm", 0, HEADERENCSW) \
    X("autoheaderencoding", 0, AUTOHEADERENCSW) \
    X("maxunencoded", 0, MAXUNENCSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("debug", -5, DEBUGSW) \
    X("dist", 0, DISTSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHBUILD);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHBUILD, switches);
#undef X

/* utf-8 is for Email Address Internationalization, using SMTPUTF8. */
#define MIMEENCODING_SWITCHES \
    X("base64", 0, BASE64SW) \
    X("quoted-printable", 0, QUOTEDPRINTSW) \
    X("utf-8", 0, UTF8SW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MIMEENCODING);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MIMEENCODING, encodingswitches);
#undef X

int debugsw = 0;

bool listsw;
bool rfc934sw;
bool contentidsw = true;

/*
 * Temporary files
 */
static char infile[BUFSIZ];
static char outfile[BUFSIZ];


int
main (int argc, char **argv)
{
    bool sizesw = true;
    bool headsw = true;
    bool directives = true;
    bool autobuild = false;
    bool dist = false;
    bool verbosw = false;
    bool dispo = false;
    size_t maxunencoded = MAXTEXTPERLN;
    char *cp, buf[BUFSIZ];
    char buffer[BUFSIZ], *compfile = NULL;
    char **argp, **arguments;
    CT ct, cts[2];
    FILE *fp = NULL;
    FILE *fp_out = NULL;
    int header_encoding = CE_UNKNOWN;
    size_t n;

    if (nmh_init(argv[0], true, false)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (cp[0] == '-' && cp[1] == '\0') {
	    if (compfile)
		die("cannot specify both standard input and a file");
            compfile = cp;
	    listsw = false;	/* turn off -list if using standard in/out */
	    verbosw = false;	/* turn off -verbose listings */
	    break;
	}
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
	    case AMBIGSW: 
		ambigsw (cp, switches);
		done (1);
	    case UNKWNSW: 
		die("-%s unknown", cp);

	    case HELPSW: 
		snprintf (buf, sizeof(buf), "%s [switches] file", invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case AUTOSW:
		/* -auto implies -nodirectives */
		autobuild = true;
		directives = false;
		continue;
	    case NAUTOSW:
		/*
		 * We're turning directives back on since this is likely here
		 * to override a profile entry
		 */
		autobuild = false;
		directives = true;
		continue;

	    case CHECKSW:
	    case NCHECKSW:
		/* Currently a NOP */
		continue;

	    case HEADSW:
		headsw = true;
		continue;
	    case NHEADSW:
		headsw = false;
		continue;

	    case DIRECTIVES:
		directives = true;
		continue;
	    case NDIRECTIVES:
		directives = false;
		continue;

	    case LISTSW:
		listsw = true;
		continue;
	    case NLISTSW:
		listsw = false;
		continue;

	    case RFC934SW:
		rfc934sw = true;
		continue;
	    case NRFC934SW:
		rfc934sw = false;
		continue;

	    case SIZESW:
		sizesw = true;
		continue;
	    case NSIZESW:
		sizesw = false;
		continue;

	    case CONTENTIDSW:
		contentidsw = true;
		continue;
	    case NCONTENTIDSW:
		contentidsw = false;
		continue;

	    case HEADERENCSW: {
		int encoding;

		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		switch (encoding = smatch (cp, encodingswitches)) {
		case AMBIGSW:
		    ambigsw (cp, encodingswitches);
		    done (1);
		case UNKWNSW:
		    die("%s unknown encoding algorithm", cp);
		case BASE64SW:
		    header_encoding = CE_BASE64;
		    break;
		case QUOTEDPRINTSW:
		    header_encoding = CE_QUOTED;
		    break;
		case UTF8SW:
		    header_encoding = CE_8BIT;
		    break;
		default:
		    die("Internal error: algorithm %s", cp);
		}
		continue;
	    }

	    case AUTOHEADERENCSW:
		header_encoding = CE_UNKNOWN;
		continue;

	    case MAXUNENCSW:
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument to %s", argp[-2]);
		if ((maxunencoded = atoi(cp)) < 1)
		    die("Invalid argument for %s: %s", argp[-2], cp);
		if (maxunencoded > 998)
		    die("limit of -maxunencoded is 998");
		continue;

	    case VERBSW: 
		verbosw = true;
		continue;
	    case NVERBSW: 
		verbosw = false;
		continue;
	    case DISPOSW:
		dispo = true;
		continue;
	    case NDISPOSW:
		dispo = false;
		continue;
	    case DEBUGSW:
		debugsw = 1;
		continue;
	    case DISTSW:
		dist = true;
		continue;
	    }
	}
	if (compfile)
	    die("only one composition file allowed");
        compfile = cp;
    }

    /*
     * Check if we've specified an additional profile
     */
    if ((cp = getenv ("MHBUILD"))) {
	if ((fp = fopen (cp, "r"))) {
	    readconfig(NULL, fp, cp, 0);
	    fclose (fp);
	} else {
	    admonish ("", "unable to read $MHBUILD profile (%s)", cp);
	}
    }

    /*
     * Read the standard profile setup
     */
    if ((fp = fopen (cp = etcpath ("mhn.defaults"), "r"))) {
	readconfig(NULL, fp, cp, 0);
	fclose (fp);
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /* Check if we have a file to process */
    if (!compfile)
	die("need to specify a %s composition file", invo_name);

    /*
     * Process the composition file from standard input.
     */
    if (compfile[0] == '-' && compfile[1] == '\0') {
	if ((cp = m_mktemp2(NULL, invo_name, NULL, &fp)) == NULL) {
	    die("unable to create temporary file in %s",
		  get_temp_dir());
	}
	strncpy (infile, cp, sizeof(infile));

	/* copy standard input to temporary file */
	while ((n = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
	    if (fwrite(buffer, 1, n, fp) != n) {
		die("error copying to temporary file");
	    }
	}
	fclose (fp);

	/* build the content structures for MIME message */
	ct = build_mime (infile, autobuild, dist, directives, header_encoding,
			 maxunencoded, verbosw);

	/*
	 * If ct == NULL, that means that -auto was set and a MIME version
	 * header was already seen.  Just use the input file as the output
	 */

	if (!ct) {
	    if (! (fp = fopen(infile, "r"))) {
		die("Unable to open %s for reading", infile);
	    }
	    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
		if (fwrite(buffer, 1, n, stdout) != n) {
		    die("error copying %s to stdout", infile);
		}
	    }
	} else {
	    /* output the message */
	    output_message_fp (ct, stdout, NULL);
	    free_content (ct);
	}

	done (0);
    }

    /*
     * Process the composition file from a file.
     */

    /* build the content structures for MIME message */
    ct = build_mime (compfile, autobuild, dist, directives, header_encoding,
		     maxunencoded, verbosw);

    /*
     * If ct == NULL, that means -auto was set and we found a MIME version
     * header.  Simply exit and do nothing.
     */

    if (! ct)
	done(0);

    cts[0] = ct;
    cts[1] = NULL;

    /* output MIME message to this temporary file */
    if ((cp = m_mktemp2(compfile, invo_name, NULL, &fp_out)) == NULL) {
	die("unable to create temporary file");
    }
    strncpy(outfile, cp, sizeof(outfile));

    /* output the message */
    output_message_fp (ct, fp_out, outfile);
    fclose(fp_out);

    /*
     * List the message info
     */
    if (listsw)
	list_all_messages (cts, headsw, sizesw, verbosw, debugsw, dispo);

    /* Rename composition draft */
    snprintf (buffer, sizeof(buffer), "%s.orig", m_backup (compfile));
    if (rename (compfile, buffer) == NOTOK) {
	adios (compfile, "unable to rename comp draft %s to", buffer);
    }

    /* Rename output file to take its place */
    if (rename (outfile, compfile) == NOTOK) {
	advise (outfile, "unable to rename output %s to", compfile);
	rename (buffer, compfile);
	done (1);
    }
    /* Remove from atexit(3) list of files to unlink. */
    if (!(m_unlink(outfile) == -1 && errno == ENOENT)) {
        adios(outfile, "file exists after rename:");
    }

    free_content (ct);
    done (0);
    return 1;
}
