/* mhlistsbr.c -- routines to list information about the
 *             -- contents of MIME messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>
#include <h/utils.h>

/* mhmisc.c */
int part_ok (CT);
int type_ok (CT, int);
void flush_errors (void);

/*
 * static prototypes
 */
static void list_single_message (CT, int, int, int, int);
static int list_debug (CT);
static int list_multi (CT, int, int, int, int, int);
static int list_partial (CT, int, int, int, int, int);
static int list_external (CT, int, int, int, int, int);
static int list_application (CT, int, int, int, int, int);
static int list_encoding (CT);


/*
 * various formats for -list option
 */
#define	LSTFMT1		"%4s %-5s %-24s %5s %s\n"
#define	LSTFMT2a	"%4d "
#define	LSTFMT2b	"%-5s %-24.24s "
#define	LSTFMT2bv	"%-5s %-24s "
#define	LSTFMT2c1	"%5lu"
#define	LSTFMT2c2	"%4lu%c"
#define	LSTFMT2c3	"huge "
#define	LSTFMT2c4	"     "
#define	LSTFMT2d1	" %.36s"
#define	LSTFMT2d1v	" %s"
#define	LSTFMT2d2	"\t     %-65s\n"


/*
 * Top level entry point to list group of messages
 */

void
list_all_messages (CT *cts, int headers, int realsize, int verbose, int debug,
		   int dispo)
{
    CT ct, *ctp;

    if (headers)
	printf (LSTFMT1, "msg", "part", "type/subtype", "size", "description");

    for (ctp = cts; *ctp; ctp++) {
	ct = *ctp;
	list_single_message (ct, realsize, verbose, debug, dispo);
    }

    flush_errors ();
}


/*
 * Entry point to list a single message
 */

static void
list_single_message (CT ct, int realsize, int verbose, int debug, int dispo)
{
    if (type_ok (ct, 1)) {
	umask (ct->c_umask);
	list_switch (ct, 1, realsize, verbose, debug, dispo);
	if (ct->c_fp) {
	    fclose (ct->c_fp);
	    ct->c_fp = NULL;
	}
	if (ct->c_ceclosefnx)
	    (*ct->c_ceclosefnx) (ct);
    }
}


/*
 * Primary switching routine to list information about a content
 */

int
list_switch (CT ct, int toplevel, int realsize, int verbose, int debug,
	     int dispo)
{
    switch (ct->c_type) {
	case CT_MULTIPART:
	    return list_multi (ct, toplevel, realsize, verbose, debug, dispo);

	case CT_MESSAGE:
	    switch (ct->c_subtype) {
		case MESSAGE_PARTIAL:
		    return list_partial (ct, toplevel, realsize, verbose,
					 debug, dispo);

		case MESSAGE_EXTERNAL:
		    return list_external (ct, toplevel, realsize, verbose,
					  debug, dispo);

		case MESSAGE_RFC822:
		default:
		    return list_content (ct, toplevel, realsize, verbose,
					 debug, dispo);
	    }

	case CT_TEXT:
	case CT_AUDIO:
	case CT_IMAGE:
	case CT_VIDEO:
	    return list_content (ct, toplevel, realsize, verbose, debug, dispo);

	case CT_APPLICATION:
	default:
	    return list_application (ct, toplevel, realsize, verbose, debug,
	    			     dispo);
    }

    return 0;	/* NOT REACHED */
}


#define empty(s) ((s) ? (s) : "")

/*
 * Method for listing information about a simple/generic content
 */

int
list_content (CT ct, int toplevel, int realsize, int verbose, int debug,
	     int dispo)
{
    unsigned long size;
    char *cp, buffer[BUFSIZ];
    CI ci = &ct->c_ctinfo;
    PM pm;

    if (toplevel > 0)
    	printf (LSTFMT2a, atoi (r1bindex (empty (ct->c_file), '/')));
    else
    	printf(toplevel < 0 ? "part " : "     ");

    snprintf (buffer, sizeof(buffer), "%s/%s", empty (ci->ci_type),
		empty (ci->ci_subtype));
    if (verbose)
	printf (LSTFMT2bv, empty (ct->c_partno), buffer);
    else
	printf (LSTFMT2b, empty (ct->c_partno), buffer);

    if (ct->c_cesizefnx && realsize)
	size = (*ct->c_cesizefnx) (ct);
    else
	size = ct->c_end - ct->c_begin;

    /* find correct scale for size (Kilo/Mega/Giga/Tera) */
    for (cp = " KMGT"; size > 9999; size /= 1000)
	if (!*++cp)
	    break;

    /* print size of this body part */
    switch (*cp) {
        case ' ':
	    if (size > 0 || ct->c_encoding != CE_EXTERNAL)
		printf (LSTFMT2c1, size);
	    else
		printf (LSTFMT2c4);
	    break;

	default:
	    printf (LSTFMT2c2, size, *cp);
	    break;

	case '\0':
	    printf (LSTFMT2c3);
    }

    /* print Content-Description */
    if (ct->c_descr) {
	char *dp;

	dp = cpytrim (ct->c_descr);
	if (verbose)
	    printf (LSTFMT2d1v, dp);
	else
	    printf (LSTFMT2d1, dp);
	free (dp);
    }

    putchar('\n');

    if (verbose) {
	CI ci = &ct->c_ctinfo;

	for (pm = ci->ci_first_pm; pm; pm = pm->pm_next) {
	    printf ("\t     %s=\"%s\"\n", pm->pm_name,
	    	    get_param_value(pm, '?'));
	}

	/*
	 * If verbose, print any RFC-822 comments in the
	 * Content-Type line.
	 */
	if (ci->ci_comment) {
	    char *dp;

	    dp = cpytrim (ci->ci_comment);
	    snprintf (buffer, sizeof(buffer), "(%s)", dp);
	    free (dp);
	    printf (LSTFMT2d2, buffer);
	}
    }

    if (dispo && ct->c_dispo_type) {
	printf ("\t     disposition \"%s\"\n", ct->c_dispo_type);

	if (verbose) {
	    for (pm = ct->c_dispo_first; pm; pm = pm->pm_next) {
		printf ("\t       %s=\"%s\"\n", pm->pm_name,
			get_param_value(pm, '?'));
	    }
	}
    }

    if (debug)
	list_debug (ct);

    return OK;
}


/*
 * Print debugging information about a content
 */

static int
list_debug (CT ct)
{
    CI ci = &ct->c_ctinfo;
    PM pm;

    fflush (stdout);
    fprintf (stderr, "  partno \"%s\"\n", empty (ct->c_partno));

    /* print MIME-Version line */
    if (ct->c_vrsn)
	fprintf (stderr, "  %s:%s\n", VRSN_FIELD, ct->c_vrsn);

    /* print Content-Type line */
    if (ct->c_ctline)
	fprintf (stderr, "  %s:%s\n", TYPE_FIELD, ct->c_ctline);

    /* print parsed elements of content type */
    fprintf (stderr, "    type    \"%s\"\n", empty (ci->ci_type));
    fprintf (stderr, "    subtype \"%s\"\n", empty (ci->ci_subtype));
    fprintf (stderr, "    comment \"%s\"\n", empty (ci->ci_comment));
    fprintf (stderr, "    magic   \"%s\"\n", empty (ci->ci_magic));

    /* print parsed parameters attached to content type */
    fprintf (stderr, "    parameters\n");
    for (pm = ci->ci_first_pm; pm; pm = pm->pm_next)
	fprintf (stderr, "      %s=\"%s\"\n", pm->pm_name,
		 get_param_value(pm, '?'));

    /* print internal flags for type/subtype */
    fprintf (stderr, "    type 0x%x subtype 0x%x params 0x%x\n",
	     ct->c_type, ct->c_subtype,
	     (unsigned int)(unsigned long) ct->c_ctparams);

    fprintf (stderr, "    showproc  \"%s\"\n", empty (ct->c_showproc));
    fprintf (stderr, "    termproc  \"%s\"\n", empty (ct->c_termproc));
    fprintf (stderr, "    storeproc \"%s\"\n", empty (ct->c_storeproc));

    /* print transfer encoding information */
    if (ct->c_celine)
	fprintf (stderr, "  %s:%s", ENCODING_FIELD, ct->c_celine);

    /* print internal flags for transfer encoding */
    fprintf (stderr, "    transfer encoding 0x%x params 0x%x\n",
	     ct->c_encoding, (unsigned int)(unsigned long) &ct->c_cefile);

    /* print Content-ID */
    if (ct->c_id)
	fprintf (stderr, "  %s:%s", ID_FIELD, ct->c_id);

    /* print Content-Description */
    if (ct->c_descr)
	fprintf (stderr, "  %s:%s", DESCR_FIELD, ct->c_descr);

    /* print Content-Disposition */
    if (ct->c_dispo)
	fprintf (stderr, "  %s:%s", DISPO_FIELD, ct->c_dispo);

    fprintf(stderr, "    disposition \"%s\"\n", empty (ct->c_dispo_type));
    fprintf(stderr, "    disposition parameters\n");
    for (pm = ct->c_dispo_first; pm; pm = pm->pm_next)
	fprintf (stderr, "      %s=\"%s\"\n", pm->pm_name,
		 get_param_value(pm, '?'));

    fprintf (stderr, "    read fp 0x%x file \"%s\" begin %ld end %ld\n",
	     (unsigned int)(unsigned long) ct->c_fp, empty (ct->c_file),
	     ct->c_begin, ct->c_end);

    /* print more information about transfer encoding */
    list_encoding (ct);

    return OK;
}

#undef empty


/*
 * list content information for type "multipart"
 */

static int
list_multi (CT ct, int toplevel, int realsize, int verbose, int debug,
	    int dispo)
{
    struct multipart *m = (struct multipart *) ct->c_ctparams;
    struct part *part;

    /* list the content for toplevel of this multipart */
    list_content (ct, toplevel, realsize, verbose, debug, dispo);

    /* now list for all the subparts */
    for (part = m->mp_parts; part; part = part->mp_next) {
	CT p = part->mp_part;

	if (part_ok (p) && type_ok (p, 1))
	    list_switch (p, 0, realsize, verbose, debug, dispo);
    }

    return OK;
}


/*
 * list content information for type "message/partial"
 */

static int
list_partial (CT ct, int toplevel, int realsize, int verbose, int debug,
	      int dispo)
{
    struct partial *p = (struct partial *) ct->c_ctparams;

    list_content (ct, toplevel, realsize, verbose, debug, dispo);
    if (verbose) {
	printf ("\t     [message %s, part %d", p->pm_partid, p->pm_partno);
	if (p->pm_maxno)
	    printf (" of %d", p->pm_maxno);
	puts("]");
    }

    return OK;
}


/*
 * list content information for type "message/external"
 */

static int
list_external (CT ct, int toplevel, int realsize, int verbose, int debug,
	       int dispo)
{
    struct exbody *e = (struct exbody *) ct->c_ctparams;

    /*
     * First list the information for the
     * message/external content itself.
     */
    list_content (ct, toplevel, realsize, verbose, debug, dispo);

    if (verbose) {
	if (e->eb_name)
	    printf ("\t     name=\"%s\"\n", e->eb_name);
	if (e->eb_dir)
	    printf ("\t     directory=\"%s\"\n", e->eb_dir);
	if (e->eb_site)
	    printf ("\t     site=\"%s\"\n", e->eb_site);
	if (e->eb_server)
	    printf ("\t     server=\"%s\"\n", e->eb_server);
	if (e->eb_subject)
	    printf ("\t     subject=\"%s\"\n", e->eb_subject);
	if (e->eb_url)
	    printf ("\t     url=\"%s\"\n", e->eb_url);

	/* This must be defined */
	printf     ("\t     access-type=\"%s\"\n", e->eb_access);

	if (e->eb_mode)
	    printf ("\t     mode=\"%s\"\n", e->eb_mode);
	if (e->eb_permission)
	    printf ("\t     permission=\"%s\"\n", e->eb_permission);

	if (e->eb_flags == NOTOK)
	    puts("\t     [service unavailable]");

    }

    /*
     * Now list the information for the external content
     * to which this content points.
     */
    list_content (e->eb_content, 0, realsize, verbose, debug, dispo);

    return OK;
}


/*
 * list content information for type "application"
 * This no longer needs to be a separate function.  It used to
 * produce some output with verbose enabled, but that has been
 * moved to list_content ().
 */

static int
list_application (CT ct, int toplevel, int realsize, int verbose, int debug,
		  int dispo)
{
    list_content (ct, toplevel, realsize, verbose, debug, dispo);

    return OK;
}


/*
 * list information about the Content-Transfer-Encoding
 * used by a content.
 */

static int
list_encoding (CT ct)
{
    CE ce = &ct->c_cefile;

    fprintf (stderr, "    decoded fp 0x%x file \"%s\"\n",
	     (unsigned int)(unsigned long) ce->ce_fp,
	     ce->ce_file ? ce->ce_file : "");

    return OK;
}
