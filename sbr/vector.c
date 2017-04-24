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

#define VEC_INIT_SIZE 256

/*
 * These try to hide the type of the "bits" member of bvector.  But if
 * that is changed to a type that's wider than unsigned long, the 1ul
 * constants in the code below must also be changed to a 1 that's at
 * least as wide as the new type.
 */
#ifndef Nbby
# define Nbby 8
#endif
#define BVEC_WORD(vec, max) ((max) / (sizeof *vec->bits * Nbby))
#define BVEC_OFFSET(vec, max) ((max) % (sizeof *vec->bits * Nbby))
#define BVEC_BYTES(vec, n) \
    ((BVEC_WORD (vec, n) + (BVEC_OFFSET (vec, n) == 0 ? 0 : 1))  * \
    sizeof *vec->bits)

struct bvector {
    unsigned long *bits;
    size_t maxsize;
};

static void bvector_resize (bvector_t, size_t);

bvector_t
bvector_create (size_t init_size) {
    bvector_t vec;
    size_t bytes;

    NEW(vec);
    bytes = BVEC_BYTES (vec, init_size  ?  init_size  :  VEC_INIT_SIZE);

    vec->bits = mh_xcalloc (1, bytes);
    vec->maxsize = bytes * Nbby;

    return vec;
}

void
bvector_copy (bvector_t dest, bvector_t src) {
    size_t bytes = BVEC_BYTES (dest, src->maxsize);

    free (dest->bits);
    dest->bits = mh_xmalloc (bytes);
    memcpy (dest->bits, src->bits, bytes);
    dest->maxsize = src->maxsize;
}

void
bvector_free (bvector_t vec) {
    free (vec->bits);
    free (vec);
}

void
bvector_clear (bvector_t vec, size_t n) {
    size_t word = BVEC_WORD (vec,n);
    size_t offset = BVEC_OFFSET (vec, n);

    if (n >= vec->maxsize)
        bvector_resize (vec, n);

    assert (sizeof *vec->bits <= sizeof 1ul);
    vec->bits[word] &= ~(1ul << offset);
}


void
bvector_clear_all (bvector_t vec) {
    memset (vec->bits, 0, BVEC_BYTES (vec, vec->maxsize));
}


void
bvector_set (bvector_t vec, size_t n) {
    size_t word = BVEC_WORD (vec, n);
    size_t offset = BVEC_OFFSET (vec, n);

    if (n >= vec->maxsize)
        bvector_resize (vec, n);
    assert (sizeof *vec->bits <= sizeof 1ul);
    vec->bits[word] |= 1ul << offset;
}

unsigned int
bvector_at (bvector_t vec, size_t i) {
    size_t word = BVEC_WORD (vec, i);
    size_t offset = BVEC_OFFSET (vec, i);

    assert (sizeof *vec->bits <= sizeof 1ul);
    return i < vec->maxsize
        ?  (vec->bits[word] & (1ul << offset) ? 1 : 0)
        :  0;
}

static void
bvector_resize (bvector_t vec, size_t maxsize) {
    size_t old_maxsize = vec->maxsize;
    size_t bytes;
    size_t i;

    while ((vec->maxsize *= 2) < maxsize)
        ;
    bytes = BVEC_BYTES (vec, vec->maxsize);
    vec->bits = mh_xrealloc (vec->bits, bytes);
    for (i = old_maxsize; i < vec->maxsize; ++i)
        bvector_clear (vec, i);
}

const unsigned long *
bvector_bits (bvector_t vec) {
    return vec->bits;
}

size_t
bvector_maxsize (bvector_t vec) {
    return vec->maxsize;
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
    vec->maxsize = init_size ? init_size : VEC_INIT_SIZE;
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
    size_t i;

    while ((vec->maxsize *= 2) < maxsize)
        ;
    vec->strs = mh_xrealloc (vec->strs, vec->maxsize * sizeof (char *));
    for (i = old_maxsize; i < vec->maxsize; ++i)
        vec->strs[i] = NULL;
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
    vec->maxsize = init_size ? init_size : VEC_INIT_SIZE;
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

size_t
ivector_size (ivector_t vec) {
    return vec->size;
}

static void
ivector_resize (ivector_t vec, size_t maxsize) {
    size_t old_maxsize = vec->maxsize;
    size_t i;

    while ((vec->maxsize *= 2) < maxsize)
        ;
    vec->ints = mh_xrealloc (vec->ints, vec->maxsize * sizeof (int));
    for (i = old_maxsize; i < vec->maxsize; ++i)
        vec->ints[i] = 0;
}
