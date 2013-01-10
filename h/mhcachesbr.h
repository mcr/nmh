
/*
 * mhcachesbr.h -- definitions for manipulating MIME content cache
 */

/*
 * various cache policies
 */
static struct swit caches[] = {
#define CACHE_NEVER    0
    { "never", 0, 0 },
#define CACHE_PRIVATE  1
    { "private", 0, 0 },
#define CACHE_PUBLIC   2
    { "public", 0, 0 },
#define CACHE_ASK      3
    { "ask", 0, 0 },
    { NULL, 0, 0 }
};
