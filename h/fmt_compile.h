/*
 * fmt_compile.h -- format types
 */

/* types that output text */
#define FT_COMP          1       /* the text of a component                 */
#define FT_COMPF         2       /* comp text, filled                       */
#define FT_LIT           3       /* literal text                            */
#define FT_LITF          4       /* literal text, filled                    */
#define FT_CHAR          5       /* a single ascii character                */
#define FT_NUM           6       /* "value" as decimal number               */
#define FT_NUMF          7       /* "value" as filled dec number            */
#define FT_STR           8       /* "str" as text                           */
#define FT_STRF          9       /* "str" as text, filled                   */
#define FT_STRFW         10      /* "str" as text, filled, width in "value" */
#define FT_STRLIT        11      /* "str" as text, no space compression     */
#define FT_STRLITZ       12      /* literal text with zero display width    */
#define FT_PUTADDR       13      /* split and print address line            */

/* types that modify the "str" or "value" registers                     */
#define FT_LS_COMP       14      /* set "str" to component text          */
#define FT_LS_LIT        15      /* set "str" to literal text            */
#define FT_LS_GETENV     16      /* set "str" to getenv(text)            */
#define FT_LS_CFIND      17      /* set "str" to context_find(text)      */
#define FT_LS_DECODECOMP 18      /* set "str" to decoded component text  */
#define FT_LS_DECODE     19      /* decode "str" as RFC-2047 header      */
#define FT_LS_TRIM       20      /* trim trailing white space from "str" */
#define FT_LV_COMP       21      /* set "value" to comp (as dec. num)    */
#define FT_LV_COMPFLAG   22      /* set "value" to comp flag word        */
#define FT_LV_LIT        23      /* set "value" to literal num           */
#define FT_LV_DAT        24      /* set "value" to dat[n]                */
#define FT_LV_STRLEN     25      /* set "value" to length of "str"       */
#define FT_LV_PLUS_L     26      /* set "value" += literal               */
#define FT_LV_MINUS_L    27      /* set "value" -= literal               */
#define FT_LV_MULTIPLY_L 28      /* set "value" to value * literal       */
#define FT_LV_DIVIDE_L   29      /* set "value" to value / literal       */
#define FT_LV_MODULO_L   30      /* set "value" to value % literal       */
#define FT_LV_CHAR_LEFT  31      /* set "value" to char left in output   */

#define FT_LS_MONTH      32      /* set "str" to tws month                   */
#define FT_LS_LMONTH     33      /* set "str" to long tws month              */
#define FT_LS_ZONE       34      /* set "str" to tws timezone                */
#define FT_LS_DAY        35      /* set "str" to tws weekday                 */
#define FT_LS_WEEKDAY    36      /* set "str" to long tws weekday            */
#define FT_LS_822DATE    37      /* set "str" to 822 date str                */
#define FT_LS_PRETTY     38      /* set "str" to pretty (?) date str         */
#define FT_LS_KILO       39      /* set "str" to "<value>[KMGT]"             */
#define FT_LS_KIBI       40      /* set "str" to "<value>[KMGT]"             */
#define FT_LV_SEC        41      /* set "value" to tws second                */
#define FT_LV_MIN        42      /* set "value" to tws minute                */
#define FT_LV_HOUR       43      /* set "value" to tws hour                  */
#define FT_LV_MDAY       44      /* set "value" to tws day of month          */
#define FT_LV_MON        45      /* set "value" to tws month                 */
#define FT_LV_YEAR       46      /* set "value" to tws year                  */
#define FT_LV_YDAY       47      /* set "value" to tws day of year           */
#define FT_LV_WDAY       48      /* set "value" to tws weekday               */
#define FT_LV_ZONE       49      /* set "value" to tws timezone              */
#define FT_LV_CLOCK      50      /* set "value" to tws clock                 */
#define FT_LV_RCLOCK     51      /* set "value" to now - tws clock           */
#define FT_LV_DAYF       52      /* set "value" to tws day flag              */
#define FT_LV_DST        53      /* set "value" to tws daylight savings flag */
#define FT_LV_ZONEF      54      /* set "value" to tws timezone flag         */

#define FT_LS_PERS       55      /* set "str" to person part of addr    */
#define FT_LS_MBOX       56      /* set "str" to mbox part of addr      */
#define FT_LS_HOST       57      /* set "str" to host part of addr      */
#define FT_LS_PATH       58      /* set "str" to route part of addr     */
#define FT_LS_GNAME      59      /* set "str" to group part of addr     */
#define FT_LS_NOTE       60      /* set "str" to comment part of addr   */
#define FT_LS_ADDR       61      /* set "str" to mbox@host              */
#define FT_LS_822ADDR    62      /* set "str" to 822 format addr        */
#define FT_LS_FRIENDLY   63      /* set "str" to "friendly" format addr */
#define FT_LV_HOSTTYPE   64      /* set "value" to addr host type       */
#define FT_LV_INGRPF     65      /* set "value" to addr in-group flag   */
#define FT_LS_UNQUOTE    66      /* remove RFC 2822 quotes from "str"   */
#define FT_LV_NOHOSTF    67      /* set "value" to addr no-host flag */

/* Date Coercion */
#define FT_LOCALDATE     68      /* Coerce date to local timezone */
#define FT_GMTDATE       69      /* Coerce date to gmt            */

/* pre-format processing */
#define FT_PARSEDATE     70      /* parse comp into a date (tws) struct */
#define FT_PARSEADDR     71      /* parse comp into a mailaddr struct   */
#define FT_FORMATADDR    72      /* let external routine format addr    */
#define FT_CONCATADDR    73      /* formataddr w/out duplicate removal  */
#define FT_MYMBOX        74      /* do "mymbox" test on comp            */
#define FT_GETMYMBOX     75      /* return "mymbox" mailbox string      */
#define FT_GETMYADDR     76      /* return "mymbox" addr string         */

/* conditionals & control flow (must be last) */
#define FT_SAVESTR       77      /* save current str reg               */
#define FT_DONE          78      /* stop formatting                    */
#define FT_PAUSE         79      /* pause                              */
#define FT_NOP           80      /* nop                                */
#define FT_GOTO          81      /* (relative) goto                    */
#define FT_IF_S_NULL     82      /* test if "str" null                 */
#define FT_IF_S          83      /* test if "str" non-null             */
#define FT_IF_V_EQ       84      /* test if "value" = literal          */
#define FT_IF_V_NE       85      /* test if "value" != literal         */
#define FT_IF_V_GT       86      /* test if "value" > literal          */
#define FT_IF_MATCH      87      /* test if "str" contains literal     */
#define FT_IF_AMATCH     88      /* test if "str" starts with literal  */
#define FT_S_NULL        89      /* V = 1 if "str" null                */
#define FT_S_NONNULL     90      /* V = 1 if "str" non-null            */
#define FT_V_EQ          91      /* V = 1 if "value" = literal         */
#define FT_V_NE          92      /* V = 1 if "value" != literal        */
#define FT_V_GT          93      /* V = 1 if "value" > literal         */
#define FT_V_MATCH       94      /* V = 1 if "str" contains literal    */
#define FT_V_AMATCH      95      /* V = 1 if "str" starts with literal */

#define IF_FUNCS FT_S_NULL       /* start of "if" functions */
