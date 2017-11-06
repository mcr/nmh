/* new.c -- as new,    list all folders with unseen messages
 *       -- as fnext,  move to next folder with unseen messages
 *       -- as fprev,  move to previous folder with unseen messages
 *       -- as unseen, scan all unseen messages
 * This code is Copyright (c) 2008, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 *
 * Inspired by Luke Mewburn's new: http://www.mewburn.net/luke/src/new
 */

#include <sys/types.h>

#include "h/mh.h"
#include "sbr/ambigsw.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include "h/crawl_folders.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/lock_file.h"
#include "sbr/m_maildir.h"

#define NEW_SWITCHES \
    X("mode", 1, MODESW) \
    X("folders", 1, FOLDERSSW) \
    X("version", 1, VERSIONSW) \
    X("help", 1, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(NEW);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(NEW, switches);
#undef X

/* What to do, based on argv[0]. */
static enum {
    RM_NEW,
    RM_FNEXT,
    RM_FPREV,
    RM_UNSEEN
} run_mode = RM_NEW;

/* check_folders uses this to maintain state with both .folders list of
 * folders and with crawl_folders. */
struct list_state {
    struct node **first, **cur_node;
    size_t *maxlen;
    char *cur;
    char **sequences;
    struct node *node;
};

/* Return the number of messages in a string list of message numbers. */
static int
count_messages(char *field)
{
    int total = 0;
    int j, k;
    char *cp, **ap;

    field = getcpy(field);

    /* copied from seq_read.c:seq_init */
    for (ap = brkstring (field, " ", "\n"); *ap; ap++) {
        if ((cp = strchr(*ap, '-')))
            *cp++ = '\0';
        if ((j = m_atoi (*ap)) > 0) {
            k = cp ? m_atoi (cp) : j;

            total += k - j + 1;
        }
    }

    free(field);

    return total;
}

/* Return true if the sequence 'name' is in 'sequences'. */
static bool
seq_in_list(char *name, char *sequences[])
{
    int i;

    for (i = 0; sequences[i] != NULL; i++) {
	if (strcmp(name, sequences[i]) == 0) {
	    return true;
	}
    }

    return false;
}

/* Return the string list of message numbers from the sequences file, or NULL
 * if none. */
static char *
get_msgnums(char *folder, char *sequences[])
{
    char *seqfile = NULL;
    FILE *fp;
    int state;
    char name[NAMESZ], field[NMH_BUFSIZ];
    char *cp;
    char *msgnums = NULL, *this_msgnums, *old_msgnums;
    int failed_to_lock = 0;
    m_getfld_state_t gstate;

    /* copied from seq_read.c:seq_public */
    /*
     * If mh_seq == NULL or if *mh_seq == '\0' (the user has defined
     * the "mh-sequences" profile entry, but left it empty),
     * then just return, and do not initialize any public sequences.
     */
    if (mh_seq == NULL || *mh_seq == '\0')
	return NULL;

    /* get filename of sequence file */
    seqfile = concat(m_maildir(folder), "/", mh_seq, NULL);

    if (seqfile == NULL)
    	return NULL;

    if ((fp = lkfopendata (seqfile, "r", & failed_to_lock)) == NULL) {
	if (failed_to_lock)
	    adios (seqfile, "failed to lock");
        free(seqfile);
        return NULL;
    }

    /* Use m_getfld2 to scan sequence file */
    gstate = m_getfld_state_init(fp);
    for (;;) {
	int fieldsz = sizeof field;
	switch (state = m_getfld2(&gstate, name, field, &fieldsz)) {
            case FLD:
            case FLDPLUS:
                if (state == FLDPLUS) {
                    cp = getcpy (field);
                    while (state == FLDPLUS) {
			fieldsz = sizeof field;
			state = m_getfld2(&gstate, name, field, &fieldsz);
                        cp = add (field, cp);
                    }

                    /* Here's where we differ from seq_public: if it's in a
		     * sequence we want, save the list of messages. */
                    if (seq_in_list(name, sequences)) {
			this_msgnums = trimcpy(cp);
			if (msgnums == NULL) {
			    msgnums = this_msgnums;
			} else {
			    old_msgnums = msgnums;
			    msgnums = concat(old_msgnums, " ",
					     this_msgnums, NULL);
			    free(old_msgnums);
			    free(this_msgnums);
			}
                    }
                    free (cp);
                } else {
		    /* and here */
                    if (seq_in_list(name, sequences)) {
			this_msgnums = trimcpy(field);
			if (msgnums == NULL) {
			    msgnums = this_msgnums;
			} else {
			    old_msgnums = msgnums;
			    msgnums = concat(old_msgnums, " ",
					     this_msgnums, NULL);
			    free(old_msgnums);
			    free(this_msgnums);
			}
                    }
                }

                continue;

            case BODY:
                die("no blank lines are permitted in %s", seqfile);
                break;

            case FILEEOF:
                break;

            default:
                die("%s is poorly formatted", seqfile);
        }
        break;  /* break from for loop */
    }
    m_getfld_state_destroy (&gstate);

    lkfclosedata (fp, seqfile);

    free(seqfile);

    return msgnums;
}

/* Check `folder' (of length `len') for interesting messages, filling in the
 * list in `b'. */
static void
check_folder(char *folder, size_t len, struct list_state *b)
{
    char *msgnums = get_msgnums(folder, b->sequences);
    int is_cur = strcmp(folder, b->cur) == 0;

    if (is_cur || msgnums != NULL) {
	if (*b->first == NULL) {
	    NEW(b->node);
	    *b->first = b->node;
	} else {
	    NEW(b->node->n_next);
	    b->node = b->node->n_next;
	}
	b->node->n_name = folder;
	b->node->n_field = msgnums;

	if (*b->maxlen < len) {
	    *b->maxlen = len;
	}
    }

    /* Save the node for the current folder, so we can fall back to it. */
    if (is_cur) {
	*b->cur_node = b->node;
    }
}

static bool
crawl_callback(char *folder, void *baton)
{
    check_folder(folder, strlen(folder), baton);
    return true;
}

/* Scan folders, returning:
 * first        -- list of nodes for all folders which have desired messages;
 *                 if the current folder is listed in .folders, it is also in
 *                 the list regardless of whether it has any desired messages
 * last         -- last node in list
 * cur_node     -- node of current folder, if listed in .folders
 * maxlen       -- length of longest folder name
 *
 * `cur' points to the name of the current folder, `folders' points to the
 * name of a .folder (if NULL, crawl all folders), and `sequences' points to
 * the array of sequences for which to look.
 *
 * An empty list is returned as first=last=NULL.
 */
static void
check_folders(struct node **first, struct node **last,
	      struct node **cur_node, size_t *maxlen,
	      char *cur, char *folders, char *sequences[])
{
    struct list_state b;
    FILE *fp;
    char *line;
    size_t len;

    *first = *last = *cur_node = NULL;
    *maxlen = 0;

    b.first = first;
    b.cur_node = cur_node;
    b.maxlen = maxlen;
    b.cur = cur;
    b.sequences = sequences;

    if (folders == NULL) {
	if (chdir(m_maildir("")) < 0) {
	    advise (m_maildir(""), "chdir");
	}
	crawl_folders(".", crawl_callback, &b);
    } else {
	fp = fopen(folders, "r");
	if (fp  == NULL) {
	    die("failed to read %s", folders);
	}
	while (vfgets(fp, &line) == OK) {
	    len = strlen(line) - 1;
	    line[len] = '\0';
	    check_folder(mh_xstrdup(line), len, &b);
	}
	fclose(fp);
    }

    if (*first != NULL) {
	b.node->n_next = NULL;
	*last = b.node;
    }
}

/* Return a single string of the `sequences' joined by a space (' '). */
static char *
join_sequences(char *sequences[])
{
    int i;
    size_t len = 0;
    char *result, *cp;

    for (i = 0; sequences[i] != NULL; i++) {
	len += strlen(sequences[i]) + 1;
    }
    result = mh_xmalloc(len + 1);

    for (i = 0, cp = result; sequences[i] != NULL; i++, cp += len + 1) {
	len = strlen(sequences[i]);
	memcpy(cp, sequences[i], len);
	cp[len] = ' ';
    }
    /* -1 to overwrite the last delimiter */
    *--cp = '\0';

    return result;
}

/* Return a struct node for the folder to change to.  This is the next
 * (previous, if RM_FPREV mode) folder with desired messages, or the current
 * folder if no folders have desired.  If RM_NEW or RM_UNSEEN mode, print the
 * output but don't change folders.
 *
 * n_name is the folder to change to, and n_field is the string list of
 * desired message numbers.
 */
static struct node *
doit(char *cur, char *folders, char *sequences[])
{
    struct node *first, *cur_node, *node, *last, *prev;
    size_t folder_len;
    int count, total = 0;
    char *command = NULL, *sequences_s = NULL;

    if (cur == NULL || cur[0] == '\0') {
        cur = "inbox";
    }

    check_folders(&first, &last, &cur_node, &folder_len, cur,
		  folders, sequences);

    if (run_mode == RM_FNEXT || run_mode == RM_FPREV) {
	if (first == NULL) {
	    /* No folders at all... */
	    return NULL;
	}
        if (first->n_next == NULL) {
	    /* We have only one node; any desired messages in it? */
	    if (first->n_field == NULL) {
		return NULL;
	    }
            return first;
	}
        if (cur_node == NULL) {
	    /* Current folder is not listed in .folders, return first. */
	    return first;
	}
    } else if (run_mode == RM_UNSEEN) {
	sequences_s = join_sequences(sequences);
    }

    for (node = first, prev = NULL;
	 node != NULL;
	 prev = node, node = node->n_next) {
        if (run_mode == RM_FNEXT) {
            /* If we have a previous node and it is the current
             * folder, return this node. */
            if (prev != NULL && strcmp(prev->n_name, cur) == 0) {
                return node;
            }
        } else if (run_mode == RM_FPREV) {
            if (strcmp(node->n_name, cur) == 0) {
                /* Found current folder in fprev mode; if we have a
                 * previous node in the list, return it; else return
                 * the last node. */
                if (prev)
                    return prev;
                return last;
            }
        } else if (run_mode == RM_UNSEEN) {
            int status;

            if (node->n_field == NULL) {
                continue;
            }

            printf("\n%d %s messages in %s",
                   count_messages(node->n_field),
		   sequences_s,
                   node->n_name);
            if (strcmp(node->n_name, cur) == 0) {
                puts(" (*: current folder)");
            } else {
                putchar('\n');
            }
            fflush(stdout);

	    /* TODO: Split enough of scan.c out so that we can call it here. */
	    command = concat("scan +", node->n_name, " ", sequences_s,
			     NULL);
	    status = system(command);
	    if (! WIFEXITED (status)) {
		adios (command, "system");
	    }
	    free(command);
        } else {
            if (node->n_field == NULL) {
                continue;
            }

            count = count_messages(node->n_field);
            total += count;

            printf("%-*s %6d.%c %s\n",
                   (int) folder_len, node->n_name,
                   count,
                   (strcmp(node->n_name, cur) == 0 ? '*' : ' '),
                   node->n_field);
        }
    }

    /* If we're fnext, we haven't checked the last node yet.  If it's the
     * current folder, return the first node. */
    if (run_mode == RM_FNEXT) {
	assert(last != NULL);
	if (strcmp(last->n_name, cur) == 0) {
            return first;
	}
    }

    if (run_mode == RM_NEW) {
        printf("%-*s %6d.\n", (int) folder_len, " total", total);
    }

    return cur_node;
}

int
main(int argc, char **argv)
{
    char **ap, *cp, **argp, **arguments;
    char help[BUFSIZ];
    char *folders = NULL;
    svector_t sequences = svector_create (0);
    int i = 0;
    char *unseen;
    struct node *folder;

    if (nmh_init(argv[0], true, true)) { return 1; }

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
		die("-%s unknown", cp);

	    case HELPSW:
		snprintf (help, sizeof(help), "%s [switches] [sequences]",
			  invo_name);
		print_help (help, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case FOLDERSSW:
		if (!(folders = *argp++) || *folders == '-')
		    die("missing argument to %s", argp[-2]);
		continue;
	    case MODESW:
		if (!(invo_name = *argp++) || *invo_name == '-')
		    die("missing argument to %s", argp[-2]);
		invo_name = r1bindex(invo_name, '/');
		continue;
	    }
	}
	/* have a sequence argument */
	if (!seq_in_list(cp, svector_strs (sequences))) {
	    svector_push_back (sequences, cp);
	    ++i;
	}
    }

    if (strcmp(invo_name, "fnext") == 0) {
        run_mode = RM_FNEXT;
    } else if (strcmp(invo_name, "fprev") == 0) {
        run_mode = RM_FPREV;
    } else if (strcmp(invo_name, "unseen") == 0) {
        run_mode = RM_UNSEEN;
    }

    if (folders == NULL) {
	/* will flists */
    } else {
	if (folders[0] != '/') {
	    folders = m_maildir(folders);
	}
    }

    if (i == 0) {
	/* no sequence arguments; use unseen */
	unseen = context_find(usequence);
	if (unseen == NULL || unseen[0] == '\0') {
	    die("must specify sequences or set %s", usequence);
	}
	for (ap = brkstring(unseen, " ", "\n"); *ap; ap++) {
	    svector_push_back (sequences, *ap);
	    ++i;
	}
    }

    folder = doit(context_find(pfolder), folders, svector_strs (sequences));
    if (folder == NULL) {
        done(0);
        return 1;
    }

    if (run_mode == RM_UNSEEN) {
        /* All the scan(1)s it runs change the current folder, so we
         * need to put it back.  Unfortunately, context_replace lamely
         * ignores the new value you give it if it is the same one it
         * has in memory.  So, we'll be lame, too.  I'm not sure if i
         * should just change context_replace... */
        context_replace(pfolder, "defeat_context_replace_optimization");
    }

    /* update current folder */
    context_replace(pfolder, folder->n_name);

    if (run_mode == RM_FNEXT || run_mode == RM_FPREV) {
        printf("%s  %s\n", folder->n_name, folder->n_field);
    }

    context_save();

    svector_free (sequences);
    done (0);
    return 1;
}
