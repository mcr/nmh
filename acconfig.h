
/****** BEGIN USER CONFIGURATION SECTION *****/

/*
 * IMPORTANT: You should no longer need to edit this file to handle
 * your operating system. That should be handled and set correctly by
 * configure now.
 *
 * These are slowly being phased out, but currently
 * not everyone is auto-configured.  Then decide if you
 * wish to change the features that are compiled into nmh.
 */

/*
 * Directs nmh not to try and rewrite addresses
 * to their official form.  You probably don't
 * want to change this without good reason.
 */
#define DUMB    1

/*
 * Define this if you do not want nmh to attach the local hostname
 * to local addresses.  You must also define DUMB.  You probably
 * don't need this unless you are behind a firewall.
 */
/* #define REALLYDUMB  1 */

/*
 * If defined, slocal will use `mbox' format when saving to
 * your standard mail spool.  If not defined, it will use
 * mmdf format.
 */
#define SLOCAL_MBOX  1

/*
 * If this is defined, nmh will recognize the ~ construct.
 */
#define MHRC    1

/*
 * Compile simple ftp client into mhn.  This will be used by mhn
 * for ftp access unless you have specified another access method
 * in your .mh_profile or mhn.defaults.  Use the "mhn-access-ftp"
 * profile entry to override this.  Check mhn(1) man page for
 * details.
 */
#define BUILTIN_FTP 1

/*
 * If you enable POP support, this is the the port name that nmh will use.  Make
 * sure this is defined in your /etc/services file (or its NIS/NIS+ equivalent).
 * If you are using KPOP, you will need to change this to "kpop" unless you want
 * to be able to use both POP3 _and_ Kerberized POP and plan to use inc and
 * msgchk's -kpop switch every time in the latter case.
 */
#define POPSERVICE "pop3"

/*
 * Define the default creation modes for folders and messages.
 */
#define DEFAULT_FOLDER_MODE "700"
#define DEFAULT_MESSAGE_MODE "600"

/*
 * Name of link to file to which you are replying.
 */
#define LINK "@"

/*
 * Define to 1 if your vi has ATT bug, such that it returns
 * non-zero exit codes on `pseudo-errors'.
 */
#undef ATTVIBUG


/***** END USER CONFIGURATION SECTION *****/
@TOP@
