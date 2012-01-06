
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
