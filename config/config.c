
/*
 * config.c -- master nmh configuration file
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <stdio.h>
#include <pwd.h>

#define nmhbindir(file) NMHBINDIR#file
#define nmhetcdir(file) NMHETCDIR#file
#define nmhlibdir(file) NMHLIBDIR#file


/*
 * Find the location of a format or configuration
 * file, and return its absolute pathname.
 *
 * 1) If already absolute pathname, then leave unchanged.
 * 2) Next, if it begins with ~user, then expand it.
 * 3) Next, check in nmh Mail directory.
 * 4) Next, check in nmh `etc' directory.
 *
 */

char *
etcpath (char *file)
{
    static char epath[PATH_MAX];
    char *cp;
    char *pp;
    struct passwd *pw;

    context_read();

    switch (*file) {
	case '/': 
	    /* If already absolute pathname, return it */
	    return file;

	case '~': 
	    /* Expand ~username */
	    if ((cp = strchr(pp = file + 1, '/')))
		*cp++ = '\0';
	    if (*pp == '\0') {
		pp = mypath;
	    } else {
		if ((pw = getpwnam (pp)))
		    pp = pw->pw_dir;
		else {
		    if (cp)
			*--cp = '/';
		    goto try_it;
		}
	    }

	    snprintf (epath, sizeof(epath), "%s/%s", pp, cp ? cp : "");
	    if (cp)
		*--cp = '/';

	    if (access (epath, R_OK) != NOTOK)
		return epath;	/* else fall */
try_it:

	default: 
	    /* Check nmh Mail directory */
	    if (access ((cp = m_mailpath (file)), R_OK) != NOTOK) {
		/* Will leak because caller doesn't know cp was
		   dynamically allocated. */
		return cp;
	    } else {
		free (cp);
	    }
    }

    /* Check nmh `etc' directory */
    snprintf (epath, sizeof(epath), nmhetcdir(/%s), file);
    return (access (epath, R_OK) != NOTOK ? epath : file);
}


/* 
 * Standard yes/no switches structure
 */

struct swit anoyes[] = {
    { "no", 0, 0 },
    { "yes", 0, 1 },
    { NULL, 0, 0 }
};

/* 
 * nmh constants
 */

/* initial profile for new users */
char *mh_defaults = nmhetcdir (/mh.profile);

/* default name of user profile */
char *mh_profile = ".mh_profile";

/* name of current message "sequence" */
char *current = "cur";

/* standard component files */
char *components = "components";		/* comp         */
char *replcomps = "replcomps";			/* repl         */
char *replgroupcomps = "replgroupcomps";	/* repl -group  */
char *forwcomps = "forwcomps";			/* forw         */
char *distcomps = "distcomps";			/* dist         */
char *rcvdistcomps = "rcvdistcomps";		/* rcvdist      */
char *digestcomps = "digestcomps";		/* forw -digest */

/* standard format (filter) files */
char *mhlformat = "mhl.format";			/* show         */
char *mhlreply = "mhl.reply";			/* repl -filter */
char *mhlforward = "mhl.forward";		/* forw -filter */

char *draft = "draft";

char *inbox = "Inbox";
char *defaultfolder = "inbox";

char *pfolder = "Current-Folder";
char *usequence = "Unseen-Sequence";
char *psequence = "Previous-Sequence";
char *nsequence = "Sequence-Negation";

/* profile entries for storage locations */
char *nmhstorage = "nmh-storage";
char *nmhcache = "nmh-cache";
char *nmhprivcache = "nmh-private-cache";

/* profile entry for external ftp access command */
char *nmhaccessftp = "nmh-access-ftp";

/* profile entry for external url access command */
char *nmhaccessurl = "nmh-access-url";

char *mhlibdir = NMHLIBDIR;
char *mhetcdir = NMHETCDIR;

/* 
 * nmh not-so constants
 */

/*
 * Default name for the nmh context file.
 */
char *context = "context";

/*
 * Default name of file for public sequences.  If NULL,
 * then nmh will use private sequences by default, unless the
 * user defines a value using the "mh-sequences" profile entry.
 */
#ifdef NOPUBLICSEQ
char *mh_seq = NULL;
#else
char *mh_seq = ".mh_sequences";
#endif

/* 
 * nmh globals
 */

char ctxflags;		/* status of user's context   */
char *invo_name;	/* command invocation name    */
char *mypath;		/* user's $HOME               */
char *defpath;		/* pathname of user's profile */
char *ctxpath;		/* pathname of user's context */
struct node *m_defs;	/* profile/context structure  */

/* 
 * nmh processes
 */

/*
 * This is the program to process MIME composition files
 */
char *buildmimeproc = nmhbindir (/mhbuild);
/*
 * This is the program to `cat' a file.
 */
char *catproc = "/bin/cat";

/*
 * This program is usually called directly by users, but it is
 * also invoked by the post program to process an "Fcc", or by
 * comp/repl/forw/dist to refile a draft message.
 */

char *fileproc = nmhbindir (/refile);

/*
 * This program is used to optionally format the bodies of messages by
 * "mhl".
 */

char *formatproc = NULL;

/* 
 * This program is called to incorporate messages into a folder.
 */

char *incproc = nmhbindir (/inc);

/*
 * This is the default program invoked by a "list" response
 * at the "What now?" prompt.  It is also used by the draft
 * folder facility in comp/dist/forw/repl to display the
 * draft message.
 */

char *lproc = NULL;

/*
 * This is the path for the Bell equivalent mail program.
 */

char *mailproc = nmhbindir (/mhmail);

/*
 * This is used by mhl as a front-end.  It is also used
 * by mhn as the default method of displaying message bodies
 * or message parts of type text/plain.
 */

char *moreproc = NULL;

/* 
 * This is the program (mhl) used to filter messages.  It is
 * used by mhn to filter and display the message headers of
 * MIME messages.  It is used by repl/forw (with -filter)
 * to filter the message to which you are replying/forwarding.
 * It is used by send/post (with -filter) to filter the message
 * for "Bcc:" recipients.
 */

char *mhlproc = nmhlibdir (/mhl);

/* 
 * This is the super handy BBoard reading program, which is
 * really just the nmh shell program.
 */

char *mshproc = nmhbindir (/msh);

/* 
 * This program is called to pack a folder.  
 */

char *packproc = nmhbindir (/packf);

/*
 * This is the delivery program called by send to actually
 * deliver mail to users.  This is the interface to the MTS.
 */

char *postproc = nmhlibdir (/post);

/*
 * This is program is called by slocal to handle
 * the action `folder' or `+'.
 */

char *rcvstoreproc = nmhlibdir (/rcvstore);

/* 
 * This program is called to remove a message by rmm or refile -nolink.
 * It's usually empty, which means to rename the file to a backup name.
 */

char *rmmproc = NULL;

/*
 * This program is usually called by the user's whatnowproc, but it
 * may also be called directly to send a message previously composed.
 */

char *sendproc = nmhbindir (/send);

/*
 * This is the path to the program used by "show"
 * to display non-text (MIME) messages.
 */

char *showmimeproc = nmhbindir (/mhshow);

/*
 * This is the default program called by "show" to filter
 * and display standard text (non-MIME) messages.  It can be
 * changed to a pager (such as "more" or "less") if you prefer
 * that such message not be filtered in any way.
 */

char *showproc = nmhlibdir (/mhl);

/* 
 * This program is called by vmh as the back-end to the window management
 * protocol
 */

char *vmhproc = nmhbindir (/msh);

/* 
 * This program is called after comp, et. al., have built a draft
 */

char *whatnowproc = nmhbindir (/whatnow);

/* 
 * This program is called to list/validate the addresses in a message.
 */

char *whomproc = nmhbindir (/whom);

/* 
 * This is the global nmh alias file.  It is somewhat obsolete, since
 * global aliases should be handled by the Mail Transport Agent (MTA).
 */

char *AliasFile = nmhetcdir (/MailAliases);

/* 
 * File protections
 */

/*
 * Folders (directories) are created with this protection (mode)
 */

char *foldprot = "700";

/*
 * Every NEW message will be created with this protection.  When a
 * message is filed it retains its protection, so this only applies
 * to messages coming in through inc.
 */

char *msgprot = "600";

