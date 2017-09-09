/* icalendar.l -- icalendar (RFC 5545) scanner
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/* See porting notes at end of this file. */

%{
#include "h/mh.h"
#include "h/icalendar.h"
#include "sbr/icalparse.h"
#include "sbr/base64.h"

static char *unfold (char *, size_t *);
static void destroy_icallex(void);
%}

/*
 * These flex options aren't used:
 *   8bit not needed
 *   case-insensitive not needed
 *   align not used because this isn't performance critical
 */
%option outfile="lex.yy.c" prefix="ical"
%option perf-report warn
%option never-interactive noinput noyywrap

              /*
               * From RFC 5545 § 3.1.
               */
name          {iana-token}|{x-name}
iana-token    ({ALPHA}|{DIGIT}|-)+
x-name        X-({vendorid}-)?({ALPHA}|{DIGIT}|-)+
vendorid      ({ALPHA}|{DIGIT}){3,}
param-name    {iana-token}|{x-name}
param-value   {paramtext}|{quoted-string}
paramtext     {SAFE-CHAR}*
value         {VALUE-CHAR}*
quoted-string {DQUOTE}{QSAFE-CHAR}*{DQUOTE}
QSAFE-CHAR    {WSP}|[\x21\x23-\x7E]|{NON-US-ASCII}
SAFE-CHAR     {WSP}|[\x21\x23-\x2B\x2D-\x39\x3C-\x7E]|{NON-US-ASCII}
VALUE-CHAR    {WSP}|[\x21-\x7E]|{NON-US-ASCII}
              /* The following is a short-cut definition that admits more
                 that the UNICODE characters permitted by RFC 5545. */
NON-US-ASCII  [\x80-\xF8]{2,4}
              /* The following excludes HTAB, unlike {CTL}. */
CONTROL       [\x00-\x08\x0A-\x1F\x7F]
EQUAL         =
              /* Solaris lex requires that the , be escaped. */
COMMA         \,
              /*
               * From RFC 5545 § 2.1.
               */
COLON         :
SEMICOLON     ;

              /*
               * From RFC 5545 § 3.3.11.
               */
text          ({TSAFE-CHAR}|:|{DQUOTE}|{ESCAPED-CHAR})*
ESCAPED-CHAR  \\\\|\\;|\\,|\\N|\\n
TSAFE-CHAR    {WSP}|[\x21\x23-\x2B\x2D-\x39\x3C-\x5B\x5D-\x7E]|{NON-US-ASCII|

              /*
               * Core rules (definitions) from RFC 5234 Appendix B.1.
               */
ALPHA         [\x41-\x5A\x61-\x7A]
BIT           [01]
CHAR          [\x01-\x7F]
CR            \x0D
              /* Variance from RFC 5234:  the {CR} is required in
                 CRLF, but it is optional below to support Unix
                 filesystem convention. */
CRLF          ({CR}?{LF})+
CTL           [\x00-\x1F\x7F]
DIGIT         [\x30-\x39]
DQUOTE        \x22
HEXDIG        {DIGIT}|[A-F]
HTAB          \x09
LF            \x0A
LWSP          ({WSP}|({CRLF}{WSP}))*
OCTET         [\x00-\xFF]
SP            \x20
VCHAR         [\x21-\x7E]
WSP           {SP}|{HTAB}

/*
 * Our definitions.
 */
fold                 {CRLF}{WSP}
folded-name          {name}({fold}+{iana-token})+
folded-param-name    {param-name}({fold}+{iana-token})+
folded-quoted-string {DQUOTE}{QSAFE-CHAR}*{fold}+{QSAFE-CHAR}*{DQUOTE}
folded-param-value   {paramtext}({fold}{paramtext}*)+|{folded-quoted-string}
folded-value         {VALUE-CHAR}*({fold}{VALUE-CHAR}*)+

%s s_name s_colon s_value s_semicolon s_param_name s_equal s_comma

%%

<INITIAL>
{CRLF} {
    /* Eat any leading newlines. */
}

<INITIAL>
{folded-name} {
    /* flex 2.5.4 defines icalleng as an int instead of a size_t,
       so copy it. */
    size_t len = icalleng;
    unfold (icaltext, &len);
    icalleng = len;

    icallval = strdup (icaltext);
    /* yy_push_state (s_name);         * s_name */
    BEGIN (s_name);                   /* s_name */
    return ICAL_NAME;
}

<INITIAL>
{name} {
    icallval = strdup (icaltext);
    /* yy_push_state (s_name);         * s_name */
    BEGIN (s_name);                   /* s_name */
    return ICAL_NAME;
}

<s_name>
{COLON} {
    /* Don't need to strdup a single character. */
    icallval = icaltext;
    /* yy_pop_state ();                * INITIAL */
    /* yy_push_state (s_colon);        * s_colon */
    BEGIN (s_colon);                  /* s_colon */
    return ICAL_COLON;
}

<s_colon>
{folded-value} {
    /* flex 2.5.4 defines icalleng as an int instead of a size_t,
       so copy it. */
    size_t len = icalleng;
    unfold (icaltext, &len);
    icalleng = len;

    icallval = strdup (icaltext);
    /* yy_pop_state ();                * INITIAL */
    /* yy_push_state (s_value);        * s_value */
    BEGIN (s_value);                  /* s_value */
    return ICAL_VALUE;
}

<s_colon>
{value} {
    icallval = strdup (icaltext);
    /* yy_pop_state ();                * INITIAL */
    /* yy_push_state (s_value);        * s_value */
    BEGIN (s_value);                  /* s_value */
    return ICAL_VALUE;
}

<s_name>
{SEMICOLON} {
    /* Don't need to strdup a single character. */
    icallval = icaltext;
    /* yy_push_state (s_semicolon);    * s_name, s_semicolon */
    BEGIN (s_semicolon);              /* s_name, s_semicolon */
    return ICAL_SEMICOLON;
}

<s_semicolon>
{folded-param-name} {
    /* flex 2.5.4 defines icalleng as an int instead of a size_t,
       so copy it. */
    size_t len = icalleng;
    unfold (icaltext, &len);
    icalleng = len;

    icallval = strdup (icaltext);
    /* yy_pop_state ();                * s_name */
    /* yy_push_state (s_param_name);   * s_name, s_param_name */
    BEGIN (s_param_name);             /* s_name, s_param_name */
    return ICAL_PARAM_NAME;
}

<s_semicolon>
{param-name} {
    icallval = strdup (icaltext);
    /* yy_pop_state ();                * s_name */
    /* yy_push_state (s_param_name);   * s_name, s_param_name */
    BEGIN (s_param_name);             /* s_name, s_param_name */
    return ICAL_PARAM_NAME;
}

<s_param_name>
{EQUAL} {
    /* Don't need to strdup a single character. */
    icallval = icaltext;
    /* yy_pop_state ();                * s_name */
    /* yy_push_state (s_equal);        * s_name, s_equal */
    BEGIN (s_equal);                  /* s_name, s_equal */
    return ICAL_EQUAL;
}

<s_equal,s_comma>
{folded-param-value} {
    /* flex 2.5.4 defines icalleng as an int instead of a size_t,
       so copy it. */
    size_t len = icalleng;
    unfold (icaltext, &len);
    icalleng = len;

    icallval = strdup (icaltext);
    /* yy_pop_state ();                * s_name */
    BEGIN (s_name);                   /* s_name */
    return ICAL_PARAM_VALUE;
}

<s_equal,s_comma>
{param-value} {
    icallval = strdup (icaltext);
    /* yy_pop_state ();                * s_name */
    BEGIN (s_name);                   /* s_name */
    return ICAL_PARAM_VALUE;
}

<s_name>
{COMMA} {
    /* Don't need to strdup a single character. */
    icallval = icaltext;
    /* yy_push_state (s_comma);        * s_name, s_comma */
    BEGIN (s_comma);                  /* s_name, s_comma */
    return ICAL_COMMA;
}

<s_value>
{CRLF} {
    /* Use start condition to ensure that all newlines are where expected. */
    icallval = icaltext;
    /* yy_pop_state ();                * INITIAL */
    BEGIN (INITIAL);                  /* INITIAL */
    return ICAL_CRLF;
}

<s_colon>
{CRLF} {
    /* Null value. */
    icallval = strdup ("");
    /* yy_pop_state ();                * INITIAL */
    /* yy_push_state (s_value);        * s_value */
    BEGIN (s_value);                  /* s_value */
    /* Push the newline back so it can be handled in the proper state. */
    unput ('\n');
    return ICAL_VALUE;
}

. {
    /* By default, flex will just pass unmatched text.  Catch it instead. */
    contentline *clines = vevents.last->contentlines;
    contentline *cline;

    if (clines  &&  (cline = clines->last)) {
        if (cline->unexpected == NULL) {
            cline->unexpected = charstring_create (0);
        }
        charstring_append_cstring (cline->unexpected, icaltext);
    }
}

<INITIAL>
<<EOF>> {
    /* See next rule for when start state is not INITIAL. */
    destroy_icallex ();
    yyterminate ();
}

<<EOF>> {
    /* Missing a final newline after a token.  The input does not conform
       to RFC 5545 § 3.1, which requires that each contentline end with a
       CRLF.  (Assume that the token is at the end of a contentline.)  Be
       liberal in what we accept by faking a newline here, and setting the
       start state to terminate on the next call. */
    BEGIN (INITIAL);
    return ICAL_CRLF;
}

%%

static char *
unfold (char *text, size_t *leng) {
    /* It's legal to shorten text and modify leng (because we don't
       use yymore()). */
    char *cp;

    /* First squash any CR-LF-WSP sequences. */
    while ((cp = strstr (text, "\r\n "))  ||  (cp = strstr (text, "\r\n\t"))) {
        /* Subtract any characters prior to fold sequence and 3 for
           the fold sequence, and add 1 for the terminating null. */
        (void) memmove (cp, cp + 3, *leng - (cp - text) - 3 + 1);
        *leng -= 3;
    }

    /* Then squash any LF-WSP sequences. */
    while ((cp = strstr (text, "\n "))  ||  (cp = strstr (text, "\n\t"))) {
        /* Subtract any characters prior to fold sequence and 2 for
           the fold sequence, and add 1 for the terminating null. */
        (void) memmove (cp, cp + 2, *leng - (cp - text) - 2 + 1);
        *leng -= 2;
    }

    return text;
}


/*
 * To clean up memory, call the function provided by modern
 * versions of flex.  Older versions don't have it, and of
 * course this won't do anything if the scanner was built
 * with something other than flex.
 */
static void
destroy_icallex(void) {
#if defined FLEX_SCANNER  &&  defined YY_FLEX_SUBMINOR_VERSION
    /* Hack:  rely on fact that the the YY_FLEX_SUBMINOR_VERSION
       #define was added to flex (flex.skl v. 2.163) after
       #yylex_destroy() was added. */
    icallex_destroy ();
#endif /* FLEX_SCANNER  &&  YY_CURRENT_BUFFER_LVALUE */
}

/*
 * See comment in h/icalendar.h about having to provide these
 * because flex 2.5.4 doesn't.
 */
void
icalset_inputfile (FILE *file) {
    yyin = file;
}

void
icalset_outputfile (FILE *file) {
    yyout = file;
}

/*
 * Porting notes
 * -------------
 * POSIX lex only supports an entry point name of yylex().  nmh
 * programs can contain multiple scanners (see sbr/dtimep.l), so
 * nmh requires the use of flex to build them.
 * In addition, if there is a need to port this to Solaris lex:
 *  - Use the lex -e or -w option.
 *  - Comment out all of the %options.
 *  - Comment out the <<EOF>> rule.
 *  - The start condition and pattern must be on the same line.
 *  - Comments must be inside rules, not just before them.
 *  - Don't use start condition stack.  In the code, above BEGIN's are
 *    used instead, and the contents of an imaginary start condition
 *    stack are shown after each.  The stack operations are also shown
 *    in comments.
 */
