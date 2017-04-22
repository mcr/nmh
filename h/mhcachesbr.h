/* mhcachesbr.h -- definitions for manipulating MIME content cache
 */

/*
 * various cache policies
 */

#define CACHE_SWITCHES \
    X("never", 0, CACHE_NEVER) \
    X("private", 0, CACHE_PRIVATE) \
    X("public", 0, CACHE_PUBLIC) \
    X("ask", 0, CACHE_ASK) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(CACHE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(CACHE, caches);
#undef X
