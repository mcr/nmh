/* vector.c -- dynamically sized vectors
 *
 * This code is Copyright (c) 2013, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 * This file defines several kinds of vectors:
 *   bvector:  bit vector
 *   svector:  vector of char arrays
 *   ivector:  vector of ints
 *
 * The interfaces provide only the capabilities needed by nmh.  The
 * implementations rely on dynamic allocation, so that a vector
 * can be as large as needed, as long as it fits in (virtual) memory.
 */

#include <h/mh.h>
#include <h/utils.h>

/* The default size of a struct bvector's bits, measured in bits.
 * The struct's tiny member is used for storage. */
#define BVEC_INIT_SIZE (sizeof *(((bvector_t)NULL)->tiny) * CHAR_BIT)

/* The default number of char pointers in a struct svector. */
#define SVEC_INIT_SIZE 256

/* The default number of ints in a struct ivector. */
#define IVEC_INIT_SIZE 256

/*
 * These try to hide the type of the "bits" member of bvector.  But if
 * that is changed to a type that's wider than unsigned long, the 1ul
 * constants in the code below must also be changed to a 1 that's at
 * least as wide as the new type.
 */

/* The *sizeof* struct bvector's bits member.  Not its size in bits. */
#define BVEC_SIZEOF_BITS (sizeof *(((bvector_t)NULL)->bits))
/* The number of bits held in one element of the bits member. */
#define BVEC_BITS_BITS (BVEC_SIZEOF_BITS * CHAR_BIT)
/* The index of bit n in struct bvector's bits member. */
#define BVEC_WORD(n) ((n) / BVEC_BITS_BITS)
/* The index of bit n within a single struct bvector's bits member. */
#define BVEC_OFFSET(n) ((n) % BVEC_BITS_BITS)
/* The number of elements bits needs to cover bit n, measured in bytes. */
#define BVEC_BYTES(n) (((n) / BVEC_BITS_BITS + 1) * BVEC_SIZEOF_BITS)

struct bvector {
    unsigned long *bits;
    size_t maxsize;
    unsigned long tiny[2];   /* Default fixed-size storage for bits. */
};

/* bvector_resize ensures the storage used for bits can cover bit
 * newsize.  It always increases the size of the storage used for bits,
 * even if newsize would have been covered by the existing storage.
 * Thus it's normally only called when it's known the storage must grow. */
static void bvector_resize (bvector_t vec, size_t newsize);

bvector_t
bvector_create (void) {
    bvector_t vec;

    /* See "wider than unsigned long" comment above. */
    assert (sizeof *vec->bits <= sizeof 1ul);

    NEW(vec);
    vec->bits = vec->tiny;
    vec->maxsize = BVEC_INIT_SIZE;
    memset(vec->tiny, 0, sizeof vec->tiny);

    return vec;
}

void
bvector_copy (bvector_t dest, bvector_t src) {
    size_t bytes = BVEC_BYTES(src->maxsize);

    if (dest->bits != dest->tiny)
        free(dest->bits);
    dest->bits = mh_xmalloc (bytes);
    memcpy (dest->bits, src->bits, bytes);
    dest->maxsize = src->maxsize;
}

void
bvector_free (bvector_t vec) {
    if (vec->bits != vec->tiny)
        free(vec->bits);
    free (vec);
}

void
bvector_clear (bvector_t vec, size_t n) {
    if (n < vec->maxsize)
        vec->bits[BVEC_WORD(n)] &= ~(1ul << BVEC_OFFSET(n));
}


void
bvector_clear_all (bvector_t vec) {
    memset (vec->bits, 0, BVEC_BYTES(vec->maxsize));
}


void
bvector_set (bvector_t vec, size_t n) {
    size_t word = BVEC_WORD(n);
    size_t offset = BVEC_OFFSET(n);

    if (n >= vec->maxsize)
        bvector_resize (vec, n);
    vec->bits[word] |= 1ul << offset;
}

unsigned int
bvector_at (bvector_t vec, size_t i) {
    if (i < vec->maxsize)
        return !!(vec->bits[BVEC_WORD(i)] & (1ul << BVEC_OFFSET(i)));

    return 0;
}

static void
bvector_resize (bvector_t vec, size_t newsize) {
    size_t oldsize = vec->maxsize;
    size_t bytes;

    while ((vec->maxsize *= 2) < newsize)
        ;
    bytes = BVEC_BYTES(vec->maxsize);
    if (vec->bits == vec->tiny) {
        vec->bits = mh_xmalloc(bytes);
        memcpy(vec->bits, vec->tiny, sizeof vec->tiny);
    } else
        vec->bits = mh_xrealloc(vec->bits, bytes);

    memset(vec->bits + (oldsize / BVEC_BITS_BITS), 0,
        (vec->maxsize - oldsize) / CHAR_BIT);
}

unsigned long
bvector_first_bits (bvector_t vec) {
    return *vec->bits;
}


struct svector {
    char **strs;
    size_t maxsize;
    size_t size;
};

static void svector_resize (svector_t, size_t);

svector_t
svector_create (size_t init_size) {
    svector_t vec;
    size_t bytes;

    NEW(vec);
    vec->maxsize = init_size ? init_size : SVEC_INIT_SIZE;
    bytes = vec->maxsize * sizeof (char *);
    vec->strs = mh_xcalloc (1, bytes);
    vec->size = 0;

    return vec;
}

void
svector_free (svector_t vec) {
    free (vec->strs);
    free (vec);
}

char *
svector_push_back (svector_t vec, char *s) {
    if (++vec->size >= vec->maxsize)
        svector_resize (vec, vec->size);
    return vec->strs[vec->size-1] = s;
}

char *
svector_at (svector_t vec, size_t i) {
    if (i >= vec->maxsize)
        svector_resize (vec, i);
    return vec->strs[i];
}

/*
 * Return address of first element that stringwise matches s.
 * The caller can replace the contents of the return address.
 */
char **
svector_find (svector_t vec, const char *s) {
    size_t i;
    char **str = vec->strs;

    for (i = 0; i < vec->size; ++i, ++str) {
        if (*str  &&  ! strcmp(*str, s)) {
            return str;
        }
    }

    return NULL;
}

char **
svector_strs (svector_t vec) {
    return vec->strs;
}

size_t
svector_size (svector_t vec) {
    return vec->size;
}

static void
svector_resize (svector_t vec, size_t maxsize) {
    size_t old_maxsize = vec->maxsize;

    while ((vec->maxsize *= 2) < maxsize)
        ;
    vec->strs = mh_xrealloc (vec->strs, vec->maxsize * sizeof (char *));
    memset(vec->strs + old_maxsize, 0,
        (vec->maxsize - old_maxsize) * sizeof *vec->strs);
}


struct ivector {
    int *ints;
    size_t maxsize;
    size_t size;
};

static void ivector_resize (ivector_t, size_t);

ivector_t
ivector_create (size_t init_size) {
    ivector_t vec;
    size_t bytes;

    NEW(vec);
    vec->maxsize = init_size ? init_size : IVEC_INIT_SIZE;
    bytes = vec->maxsize * sizeof (int);
    vec->ints = mh_xcalloc (1, bytes);
    vec->size = 0;

    return vec;
}

void
ivector_free (ivector_t vec) {
    free (vec->ints);
    free (vec);
}

int
ivector_push_back (ivector_t vec, int n) {
    if (++vec->size >= vec->maxsize)
        ivector_resize (vec, vec->size);
    return vec->ints[vec->size-1] = n;
}

int
ivector_at (ivector_t vec, size_t i) {
    if (i >= vec->maxsize)
        ivector_resize (vec, i);
    return vec->ints[i];
}

int *
ivector_atp (ivector_t vec, size_t i) {
    if (i >= vec->maxsize)
        ivector_resize (vec, i);
    return &vec->ints[i];
}

static void
ivector_resize (ivector_t vec, size_t maxsize) {
    size_t old_maxsize = vec->maxsize;

    while ((vec->maxsize *= 2) < maxsize)
        ;
    vec->ints = mh_xrealloc (vec->ints, vec->maxsize * sizeof (int));
    memset(vec->ints + old_maxsize, 0,
        (vec->maxsize - old_maxsize) * sizeof *vec->ints);
}
