%name-prefix "addr"

/*
 * Comments on these tokens:
 *
 * ATEXT is defined in RFC 5222 as:
 *	ALPHA / DIGIT /
 *	'!' / '#' / '$' / '%' / '&' / ''' / '*' / '+' / '-' / '/' /
 *      '=' / '?' / '^' / '_' / '`' / '{' / '|' / '}' / '~'
 *
 * All printable ASCII characters except for spaces and specials
 *
 * QSTRING is a quoted string, which is printable ASCII characters except
 * for \ or the quote character surrounded by quotes.  Use \ for quoting
 * \ and the quote character.
 *
 * FWS is folding white space, which is defined as SP (\040), HTAB (\011),
 * and NL (\012).  Technically CR (\015) is part of that, but traditionally
 * Unix format files don't have that character.
 *
 * COMMENT is a comment string, which is printable ASCII characters except
 * for '(', ')', and '\'.  Uses same quoting rules as QSTRING.  To make
 * the grammer slightly less conflict-happy, COMMENT must include any FWS
 * in front or behind of it (simply have it eaten in the lexer).
 *
 * Everything else is a SPECIAL, which is returned directly.  These are
 * defined in RFC 5322 as:
 *
 *	'(' / ')' / '<' / '>' / '[' / ']' / ':' / ';' / '@' / '\' / ',' / '.' /
 *	'"'
 *
 * Technically we don't return all of these; we handle () in comments, " in
 * quoted string handling, and \ in those handlers.
 */

%token ATEXT QSTRING FWS COMMENT

%%

/*
 * A list of addresses; the main entry point to the parser
 */
address_list:	/* nothing */
	| address_list ',' address
	;

/*
 * A single address; can be a single mailbox, or a group address
 */

address:
	mailbox
	| group
	;

/*
 * A traditional single mailbox.  Either in Name <user@name> or just a bare
 * email address with no angle brackets.
 */

mailbox:
	name_addr
	| addr_spec
	;

/*
 * An email address, with the angle brackets.  Optionally contains a display
 * name in the front.  The RFC says "display-name", but display-name is
 * defined as a phrase, so we just use that.
 */

name_addr:
	phrase angle_addr
	| angle_addr
	;

angle_addr:
	cfws '<' addr_spec '>' cfws
	| cfws '<' addr_spec '>'
	| '<' addr_spec '>' cfws
	| '<' addr_spec '>'
	;

/*
 * The group list syntax.  The group list is allowed to be empty or be
 * spaces, so we define group_list as either being a mailbox list or
 * just being CFWS.  mailbox_list can be empty, so that can handle the
 * case of nothing being between the ':' and the ';'
 */
group:
	phrase ':' group_list ';' cfws
	| phrase ':' group_list ';'
	| phrase ':' ';'
	;

group_list:
	mailbox_list
	| cfws
	;

mailbox_list:	/* nothing */
	| mailbox_list ',' mailbox
	;

addr_spec:
	local_part '@' domain
	;

local_part:
	dot_atom
	| quoted_string
	;

domain:
	dot_atom
	| domain_literal
	;

domain_literal:
	cfws '[' dtext_fws ']' cfws
	| cfws '[' dtext_fws ']'
	| '[' dtext_fws ']' cfws
	| '[' dtext_fws ']'
	;

/*
 * It was hard to make a definition of dtext and domain-literal that
 * exactly matched the RFC.  This was the best I could come up with.
 */
 
dtext_fws: /* nothing */
	| FWS ATEXT FWS
	| FWS ATEXT
	| ATEXT FWS
	| dtext_fws FWS ATEXT FWS
	| dtext_fws FWS ATEXT
	| dtext_fws ATEXT FWS
	| dtext_fws ATEXT
	;

phrase:
	word
	| phrase word
	| obs_phrase
	;

/*
 * obs-phrase is basically the same as "phrase", but after the first word
 * you're allowed to have a '.'.  I believe this is correct.
 */

obs_phrase:
	word obs_phrase_list
	;

obs_phrase_list:
	word
	| '.'
	| obs_phrase_list word
	;

word:
	atom
	| quoted_string
	;

/*
 * This makes sure any comments and white space before/after the quoted string
 * get eaten.
 */
quoted_string:
	cfws QSTRING cfws
	| QSTRING cfws
	| cfws QSTRING
	| QSTRING
	;

atom:
	cfws ATEXT cfws
	| cfws ATEXT
	| ATEXT cfws
	| ATEXT
	;

/*
 * Making dot-atom work was a little confusing; I finally handled it by
 * defining "dot_atom_text" as having two or more ATEXTs separted by
 * '.', and defining dot_atom as allowing a single atom.
 */
dot_atom:
	atom
	| cfws dot_atom_text cfws
	| cfws dot_atom_text
	| dot_atom_text cfws
	| dot_atom_text
	;

dot_atom_text:
	ATEXT '.' ATEXT
	| dot_atom_text '.' ATEXT
	;

/*
 * As mentioned above, technically in the CFWS definition in the RFC allows
 * FWS before and after the comment.  The lexer is responsible for eating
 * the FWS before/after comments.
 */
cfws:
	COMMENT
	| FWS
	;
