/* mime.h -- definitions for MIME
 */

#define	VRSN_FIELD	"MIME-Version"
#define	VRSN_VALUE	"1.0"
#define	XXX_FIELD_PRF	"Content-"
#define	TYPE_FIELD	"Content-Type"
#define	ENCODING_FIELD	"Content-Transfer-Encoding"
#define	ID_FIELD	"Content-ID"
#define	DESCR_FIELD	"Content-Description"
#define	DISPO_FIELD	"Content-Disposition"
#define	PSEUDOHEADER_PREFIX        "Nmh-"
#define	ATTACH_FIELD               PSEUDOHEADER_PREFIX "Attach"
#define	ATTACH_FIELD_ALT           "Attach"
#define	MHBUILD_FILE_PSEUDOHEADER  PSEUDOHEADER_PREFIX "mhbuild-file-"
#define	MHBUILD_ARGS_PSEUDOHEADER  PSEUDOHEADER_PREFIX "mhbuild-args-"

/*
 * Test for valid characters used in "token"
 * as defined in RFC2045
 */
#define	istoken(c)  (isascii((unsigned char) c) \
                     && !isspace ((unsigned char) c) \
		     && !iscntrl ((unsigned char) c) && (c) != '(' \
	             && (c) != ')' && (c) != '<'  && (c) != '>' \
	             && (c) != '@' && (c) != ','  && (c) != ';' \
	             && (c) != ':' && (c) != '\\' && (c) != '"' \
	             && (c) != '/' && (c) != '['  && (c) != ']' \
	             && (c) != '?' && (c) != '=')

/*
 * Definitions for RFC 2231 encoding
 */
#define istspecial(c)  ((c) == '(' || (c) == ')' || (c) == '<' || (c) == '>' \
		        || (c) == '@' || (c) == ',' || (c) == ';' \
			|| (c) == ':' || (c) == '\\' || (c) == '"' \
			|| (c) == '/' || (c) == '[' || (c) == ']' \
			|| (c) == '?' || (c) == '=')

#define isparamencode(c)  (!isascii((unsigned char) c) || \
			   iscntrl((unsigned char) c) || istspecial(c) || \
			   (c) == ' ' || (c) == '*' || (c) == '\'' || \
			   (c) == '%')

#define	MAXTEXTPERLN 78
#define	MAXLONGLINE 998
#define	CPERLIN	76
#define	BPERLIN	(CPERLIN / 4)
#define	LPERMSG	632
#define	CPERMSG	(LPERMSG * CPERLIN)
