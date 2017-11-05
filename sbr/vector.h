/* vector.h -- dynamically sized vectors
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

/* A vector of bits for tracking the sequence membership of a single
 * message.  Do not access the struct members; use vector.c.
 * Do not move or copy this struct as it may contain a pointer to
 * itself;  use bvector_copy(). */
struct bvector {
    unsigned long *bits;
    size_t maxsize;
    unsigned long tiny[2];   /* Default fixed-size storage for bits. */
};
typedef struct bvector *bvector_t;

bvector_t bvector_create(void);
void bvector_init(struct bvector *) NONNULL(1);
void bvector_copy(bvector_t, bvector_t) NONNULL(1, 2);
void bvector_free(bvector_t) NONNULL(1);
void bvector_fini(struct bvector *) NONNULL(1);
void bvector_clear(bvector_t, size_t) NONNULL(1);
void bvector_clear_all(bvector_t) NONNULL(1);
void bvector_set(bvector_t, size_t) NONNULL(1);
unsigned int bvector_at(bvector_t, size_t) NONNULL(1) PURE;
unsigned long bvector_first_bits(bvector_t) NONNULL(1) PURE;

typedef struct svector *svector_t;

svector_t svector_create(size_t);
void svector_free(svector_t) NONNULL(1);
char *svector_push_back(svector_t, char *) NONNULL(1);
char *svector_at(svector_t, size_t) NONNULL(1);
char **svector_find(svector_t, const char *) NONNULL(1) PURE;
char **svector_strs(svector_t) NONNULL(1) PURE;
size_t svector_size(svector_t) NONNULL(1) PURE;

typedef struct ivector *ivector_t;

ivector_t ivector_create(size_t);
void ivector_free(ivector_t) NONNULL(1);
int ivector_push_back(ivector_t, int) NONNULL(1);
int ivector_at(ivector_t, size_t) NONNULL(1);
int *ivector_atp(ivector_t, size_t) NONNULL(1);
