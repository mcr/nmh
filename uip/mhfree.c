
/*
 * mhfree.c -- routines to free the data structures used to
 *          -- represent MIME messages
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <errno.h>
#include <h/mime.h>
#include <h/mhparse.h>

/*
 * prototypes
 */
void free_content (CT);
void free_header (CT);
void free_ctinfo (CT);
void free_encoding (CT, int);

/*
 * static prototypes
 */
static void free_text (CT);
static void free_multi (CT);
static void free_partial (CT);
static void free_external (CT);


/*
 * Primary routine to free a MIME content structure
 */

void
free_content (CT ct)
{
    if (!ct)
	return;

    /*
     * free all the header fields
     */
    free_header (ct);

    if (ct->c_partno)
	free (ct->c_partno);

    if (ct->c_vrsn)
	free (ct->c_vrsn);

    if (ct->c_ctline)
	free (ct->c_ctline);

    free_ctinfo (ct);

    /*
     * some of the content types have extra
     * parts which need to be freed.
     */
    switch (ct->c_type) {
	case CT_MULTIPART:
	    free_multi (ct);
	    break;

	case CT_MESSAGE:
	    switch (ct->c_subtype) {
		case MESSAGE_PARTIAL:
		    free_partial (ct);
		    break;

		case MESSAGE_EXTERNAL:
		    free_external (ct);
		    break;
	    }
	    break;

	case CT_TEXT:
	    free_text (ct);
	    break;
    }

    if (ct->c_showproc)
	free (ct->c_showproc);
    if (ct->c_termproc)
	free (ct->c_termproc);
    if (ct->c_storeproc)
	free (ct->c_storeproc);

    if (ct->c_celine)
	free (ct->c_celine);

    /* free structures for content encodings */
    free_encoding (ct, 1);

    if (ct->c_id)
	free (ct->c_id);
    if (ct->c_descr)
	free (ct->c_descr);
    if (ct->c_dispo)
	free (ct->c_dispo);

    if (ct->c_file) {
	if (ct->c_unlink)
	    unlink (ct->c_file);
	free (ct->c_file);
    }
    if (ct->c_fp)
	fclose (ct->c_fp);

    if (ct->c_storage)
	free (ct->c_storage);
    if (ct->c_folder)
	free (ct->c_folder);

    free (ct);
}


/*
 * Free the linked list of header fields
 * for this content.
 */

void
free_header (CT ct)
{
    HF hp1, hp2;

    hp1 = ct->c_first_hf;
    while (hp1) {
	hp2 = hp1->next;

	free (hp1->name);
	free (hp1->value);
	free (hp1);

	hp1 = hp2;
    }

    ct->c_first_hf = NULL;
    ct->c_last_hf  = NULL;
}


void
free_ctinfo (CT ct)
{
    char **ap;
    CI ci;

    ci = &ct->c_ctinfo;
    if (ci->ci_type) {
	free (ci->ci_type);
	ci->ci_type = NULL;
    }
    if (ci->ci_subtype) {
	free (ci->ci_subtype);
	ci->ci_subtype = NULL;
    }
    for (ap = ci->ci_attrs; *ap; ap++) {
	free (*ap);
	*ap = NULL;
    }
    if (ci->ci_comment) {
	free (ci->ci_comment);
	ci->ci_comment = NULL;
    }
    if (ci->ci_magic) {
	free (ci->ci_magic);
	ci->ci_magic = NULL;
    }
}


static void
free_text (CT ct)
{
    struct text *t;

    if (!(t = (struct text *) ct->c_ctparams))
	return;

    free ((char *) t);
    ct->c_ctparams = NULL;
}


static void
free_multi (CT ct)
{
    struct multipart *m;
    struct part *part, *next;

    if (!(m = (struct multipart *) ct->c_ctparams))
	return;

    if (m->mp_start)
	free (m->mp_start);
    if (m->mp_stop)
	free (m->mp_stop);
	
    for (part = m->mp_parts; part; part = next) {
	next = part->mp_next;
	free_content (part->mp_part);
	free ((char *) part);
    }
    m->mp_parts = NULL;

    free ((char *) m);
    ct->c_ctparams = NULL;
}


static void
free_partial (CT ct)
{
    struct partial *p;

    if (!(p = (struct partial *) ct->c_ctparams))
	return;

    if (p->pm_partid)
	free (p->pm_partid);

    free ((char *) p);
    ct->c_ctparams = NULL;
}


static void
free_external (CT ct)
{
    struct exbody *e;

    if (!(e = (struct exbody *) ct->c_ctparams))
	return;

    free_content (e->eb_content);
    if (e->eb_body)
	free (e->eb_body);

    free ((char *) e);
    ct->c_ctparams = NULL;
}


/*
 * Free data structures related to encoding/decoding
 * Content-Transfer-Encodings.
 */

void
free_encoding (CT ct, int toplevel)
{
    CE ce;

    if (!(ce = ct->c_cefile))
	return;

    if (ce->ce_fp) {
	fclose (ce->ce_fp);
	ce->ce_fp = NULL;
    }

    if (ce->ce_file) {
	if (ce->ce_unlink)
	    unlink (ce->ce_file);
	free (ce->ce_file);
	ce->ce_file = NULL;
    }

    if (toplevel) {
	free ((char *) ce);
	ct->c_cefile = NULL;
    } else {
	ct->c_ceopenfnx = NULL;
    }
}
