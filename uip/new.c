
/*
 * new.c -- as new,    list all folders with unseen messages
 *       -- as fnext,  move to next folder with unseen messages
 *       -- as fprev,  move to previous folder with unseen messages
 *       -- as unseen, scan all unseen messages
 * $Id$
 *
 * This code is Copyright (c) 2008, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 *
 * Inspired by Luke Mewburn's new: http://www.mewburn.net/luke/src/new
 */

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <h/mh.h>
#include <h/crawl_folders.h>
#include <h/utils.h>

static struct swit switches[] = {
#define MODESW 0
    { "mode", 1 },
#define FOLDERSSW 1
    { "folders", 1 },
#define VERSIONSW 2
    { "version", 1 },
#define HELPSW 3
    { "help", 1 },
    { NULL, 0 }
};

static enum { NEW, FNEXT, FPREV, UNSEEN } run_mode = NEW;

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

/* Return TRUE if the sequence 'name' is in 'sequences'. */
static boolean
seq_in_list(char *name, char *sequences[])
{
    int i;

    for (i = 0; sequences[i] != NULL; i++) {
	if (strcmp(name, sequences[i]) == 0) {
	    return TRUE;
	}
    }

    return FALSE;
}

/* Return the string list of message numbers from the sequences file, or NULL
 * if none. */
static char *
get_msgnums(char *folder, char *sequences[])
{
    char *seqfile = concat(m_maildir(folder), "/", mh_seq, (void *)NULL);
    FILE *fp = fopen(seqfile, "r");
    int state;
    char name[NAMESZ], field[BUFSIZ];
    char *cp;
    char *msgnums = NULL, *this_msgnums, *old_msgnums;

    /* no sequences file -> no messages */
    if (fp == NULL) {
        return NULL;
    }

    /* copied from seq_read.c:seq_public */
    for (state = FLD;;) {
        switch (state = m_getfld (state, name, field, sizeof(field), fp)) {
            case FLD:
            case FLDPLUS:
            case FLDEOF:
                if (state == FLDPLUS) {
                    cp = getcpy (field);
                    while (state == FLDPLUS) {
                        state = m_getfld (state, name, field,
                                          sizeof(field), fp);
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
					     this_msgnums, (void *)NULL);
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
					     this_msgnums, (void *)NULL);
			    free(old_msgnums);
			    free(this_msgnums);
			}
                    }
                }

                if (state == FLDEOF)
                    break;
                continue;

            case BODY:
            case BODYEOF:
                adios (NULL, "no blank lines are permitted in %s", seqfile);
                /* fall */

            case FILEEOF:
                break;

            default:
                adios (NULL, "%s is poorly formatted", seqfile);
        }
        break;  /* break from for loop */
    }

    fclose(fp);

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
	    *b->first = b->node = mh_xmalloc(sizeof(*b->node));
	} else {
	    b->node->n_next = mh_xmalloc(sizeof(*b->node));
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

static boolean
crawl_callback(char *folder, void *baton)
{
    check_folder(folder, strlen(folder), baton);
    return TRUE;
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

    *first = *cur_node = NULL;
    *maxlen = 0;

    b.first = first;
    b.cur_node = cur_node;
    b.maxlen = maxlen;
    b.cur = cur;
    b.sequences = sequences;

    if (folders == NULL) {
	chdir(m_maildir(""));
	crawl_folders(".", crawl_callback, &b);
    } else {
	fp = fopen(folders, "r");
	if (fp  == NULL) {
	    adios(NULL, "failed to read %s", folders);
	}
	while (vfgets(fp, &line) == OK) {
	    len = strlen(line) - 1;
	    line[len] = '\0';
	    check_folder(getcpy(line), len, &b);
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
 * (previous, if FPREV mode) folder with desired messages, or the current
 * folder if no folders have desired.  If NEW or UNSEEN mode, print the
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
    char *command, *sequences_s;

    if (cur == NULL || cur[0] == '\0') {
        cur = "inbox";
    }

    check_folders(&first, &last, &cur_node, &folder_len, cur,
		  folders, sequences);

    if (run_mode == FNEXT || run_mode == FPREV) {
	if (first->n_next == NULL) {
	    /* We have only one node; any desired messages in it? */
	    if (first->n_field == NULL) {
		return NULL;
	    } else {
		return first;
	    }
	} else if (cur_node == NULL) {
	    /* Current folder is not listed in .folders, return first. */
	    return first;
	}
    } else if (run_mode == UNSEEN) {
	sequences_s = join_sequences(sequences);
    }

    for (node = first, prev = NULL;
	 node != NULL;
	 prev = node, node = node->n_next) {
        if (run_mode == FNEXT) {
            /* If we have a previous node and it is the current
             * folder, return this node. */
            if (prev != NULL && strcmp(prev->n_name, cur) == 0) {
                return node;
            }
        } else if (run_mode == FPREV) {
            if (strcmp(node->n_name, cur) == 0) {
                /* Found current folder in fprev mode; if we have a
                 * previous node in the list, return it; else return
                 * the last node. */
                if (prev == NULL) {
                    return last;
                }
                return prev;
            }
        } else if (run_mode == UNSEEN) {
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
                puts("");
            }
            fflush(stdout);

	    /* TODO: Split enough of scan.c out so that we can call it here. */
	    command = concat("scan +", node->n_name, " ", sequences_s,
			     (void *)NULL);
	    system(command);
	    free(command);
        } else {
            if (node->n_field == NULL) {
                continue;
            }

            count = count_messages(node->n_field);
            total += count;

            printf("%-*s %6d.%c %s\n",
                   folder_len, node->n_name,
                   count,
                   (strcmp(node->n_name, cur) == 0 ? '*' : ' '),
                   node->n_field);
        }
    }

    /* If we're fnext, we haven't checked the last node yet.  If it's the
     * current folder, return the first node. */
    if (run_mode == FNEXT && strcmp(last->n_name, cur) == 0) {
        return first;
    }

    if (run_mode == NEW) {
        printf("%-*s %6d.\n", folder_len, " total", total);
    }

    return cur_node;
}

int
main(int argc, char **argv)
{
    char **ap, *cp, **argp, **arguments;
    char help[BUFSIZ];
    char *folders = NULL;
    char *sequences[NUMATTRS + 1];
    int i = 0;
    char *unseen;
    struct node *folder;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex(argv[0], '/');

    /* read user profile/context */
    context_read();

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
		snprintf (help, sizeof(help), "%s [switches] [sequences]",
			  invo_name);
		print_help (help, switches, 1);
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

	    case FOLDERSSW:
		if (!(folders = *argp++) || *folders == '-')
		    adios(NULL, "missing argument to %s", argp[-2]);
		continue;
	    case MODESW:
		if (!(invo_name = *argp++) || *invo_name == '-')
		    adios(NULL, "missing argument to %s", argp[-2]);
		invo_name = r1bindex(invo_name, '/');
		continue;
	    }
	}
	/* have a sequence argument */
	if (!seq_in_list(cp, sequences)) {
	    sequences[i++] = cp;
	}
    }

    if (strcmp(invo_name, "fnext") == 0) {
        run_mode = FNEXT;
    } else if (strcmp(invo_name, "fprev") == 0) {
        run_mode = FPREV;
    } else if (strcmp(invo_name, "unseen") == 0) {
        run_mode = UNSEEN;
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
	    adios(NULL, "must specify sequences or set %s", usequence);
	}
	for (ap = brkstring(unseen, " ", "\n"); *ap; ap++) {
	    sequences[i++] = *ap;
	}
    }
    sequences[i] = NULL;

    folder = doit(context_find(pfolder), folders, sequences);
    if (folder == NULL) {
        done(0);
        return 1;
    }

    if (run_mode == UNSEEN) {
        /* All the scan(1)s it runs change the current folder, so we
         * need to put it back.  Unfortunately, context_replace lamely
         * ignores the new value you give it if it is the same one it
         * has in memory.  So, we'll be lame, too.  I'm not sure if i
         * should just change context_replace... */
        context_replace(pfolder, "defeat_context_replace_optimization");
    }

    /* update current folder */
    context_replace(pfolder, folder->n_name);

    if (run_mode == FNEXT || run_mode == FPREV) {
        printf("%s  %s\n", folder->n_name, folder->n_field);
    }

    context_save();

    done (0);
    return 1;
}
