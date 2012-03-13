
/*
 * fmt_scan.h -- definitions for fmt_scan()
 */

/*
 * This structure describes an "interesting" component.  It holds
 * the name & text from the component (if found) and one piece of
 * auxilary info.  The structure for a particular component is located
 * by (open) hashing the name and using it as an index into the ptr array
 * "wantcomp".  All format entries that reference a particular component
 * point to its comp struct (so we only have to do component specific
 * processing once.  e.g., parse an address.).
 */
struct comp {
    char        *c_name;	/* component name (in lower case) */
    char        *c_text;	/* component text (if found)      */
    struct comp *c_next;	/* hash chain linkage             */
    short        c_flags;	/* misc. flags (from fmt_scan)    */
    short        c_type;	/* type info   (from fmt_compile) */
    union {
	struct tws *c_u_tws;
	struct mailname *c_u_mn;
    } c_un;
};

#define c_tws c_un.c_u_tws
#define c_mn  c_un.c_u_mn

/*
 * c_type bits
 */
#define	CT_ADDR       (1<<0)	/* referenced as address    */
#define	CT_DATE       (1<<1)	/* referenced as date       */

/*
 * c_flags bits
 */
#define	CF_TRUE       (1<<0)	/* usually means component is present */
#define	CF_PARSED     (1<<1)	/* address/date has been parsed */
#define	CF_DATEFAB    (1<<2)	/* datefield fabricated */

extern int fmt_norm;

/*
 * Hash table for deciding if a component is "interesting".
 */
extern struct comp *wantcomp[128];

/* 
 * Hash function for component name.  The function should be
 * case independent and probably shouldn't involve a routine
 * call.  This function is pretty good but will not work on
 * single character component names.  
 */
#define	CHASH(nm) (((((nm)[0]) - ((nm)[1])) & 0x1f) + (((nm)[2]) & 0x5f))

/*
 * Find a component in the hash table.
 */
#define FINDCOMP(comp,name) \
		for (comp = wantcomp[CHASH(name)]; \
		     comp && strcmp(comp->c_name,name); \
		     comp = comp->c_next) ;

/*
 * This structure defines one formatting instruction.
 */
struct format {
    unsigned char f_type;
    char          f_fill;
    short         f_width;	/* output field width   */
    union {
	struct comp *f_u_comp;	/* associated component */
	char        *f_u_text;	/* literal text         */
	char         f_u_char;	/* literal character    */
	int          f_u_value;	/* literal value        */
    } f_un;
};

#define f_skip f_width		/* instr to skip (false "if") */

#define f_comp  f_un.f_u_comp
#define f_text  f_un.f_u_text
#define f_char  f_un.f_u_char
#define f_value f_un.f_u_value

/*
 * prototypes
 */
struct format *fmt_scan (struct format *, char *, int, int *);
char *new_fs (char *, char *, char *);
int fmt_compile (char *, struct format **);
char *formataddr(char *, char *);
char *concataddr(char *, char *);
