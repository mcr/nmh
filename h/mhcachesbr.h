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
extern struct swit *cache_policy;

void cache_all_messages(CT *cts);
int find_cache(CT ct, int policy, int *writing, char *id,
    char *buffer, int buflen);

extern int rcachesw;
extern int wcachesw;

extern char *cache_public;
extern char *cache_private;
