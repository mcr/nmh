/* icalendar.h -- data structures and common code for icalendar scanner,
 *                parser, and application code
 *
 * This code is Copyright (c) 2014, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 * Types used in struct contentline below.
 */
typedef struct value_list {
    char *value;
    struct value_list *next;
} value_list;

typedef struct param_list {
    char *param_name;          /* Name of property parameter. */
    struct value_list *values; /* List of its values. */
    struct param_list *next;   /* Next node in list of property parameters. */
} param_list;

typedef enum cr_indicator {
    CR_UNSET = 0,
    LF_ONLY,
    CR_BEFORE_LF
} cr_indicator;

/*
 * Each (unfolded) line in the .ics file is represented by a struct
 * contentline.
 */
typedef struct contentline {
    char *name;                /* The name of the property, calprops,
                                  or BEGIN. */
    struct param_list *params; /* List parameters. */
    char *value;               /* Everything after the ':'. */

    char *input_line;          /* The (unfolded) input line. */
    size_t input_line_len;     /* Amount of text stored in input_line. */
    size_t input_line_size;    /* Size of allocated input_line. */
    charstring_t unexpected;   /* Accumulate unexpected characters in input. */

    cr_indicator cr_before_lf; /* To support CR before LF.  If the first
                                  line of the input has a CR before its LF,
                                  assume that all output lines need to.
                                  Only used in root node. */

    struct contentline *next;  /* Next node in list of content lines. */
    struct contentline *last;  /* Last node of list.  Only used in root node. */
} contentline;


/*
 * List of vevents, each of which is a list of contentlines.
 */
typedef struct vevent {
    contentline *contentlines;
    struct vevent *next;
    /* The following is only used in the root node. */
    struct vevent *last;
} vevent;


/*
 * Exported functions.
 */
void remove_contentline (contentline *);
contentline *add_contentline (contentline *, const char *);
void add_param_name (contentline *, char *);
void add_param_value (contentline *, char *);
void remove_value (value_list *);
struct contentline *find_contentline (contentline *, const char *,
                                      const char *);
void free_contentlines (contentline *);

typedef struct tzdesc *tzdesc_t;
tzdesc_t load_timezones (const contentline *);
void free_timezones (tzdesc_t);
char *format_datetime (tzdesc_t, const contentline *);

/*
 * The following provide access to, and by, the iCalendar parser.
 */

/* This YYSTYPE definition prevents problems with Solaris yacc, which
   has declarations of two variables of type YYSTYPE * in one
   statement. */
typedef char *charptr;
#define YYSTYPE charptr

extern int icaldebug;
int icalparse (void);
extern vevent vevents;
int icallex (void);
extern int parser_status;

/* And this is for the icalendar scanner. */
extern YYSTYPE icallval;

/*
 * For directing the scanner to use files other than stdin/stdout.
 * These don't use the accessors provided by modern flex because
 * flex 2.5.4 doesn't supply them.
 */
void icalset_inputfile (FILE *);
void icalset_outputfile (FILE *);
