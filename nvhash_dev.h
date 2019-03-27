#ifndef __FROST_NV_HASH_DEV__
#define __FROST_NV_HASH_DEV__

#define BUCKET_SLOTS (4)

#include <stddef.h>
#include <libpmemobj.h>
#include "string.h"

POBJ_LAYOUT_BEGIN(nvhash);
POBJ_LAYOUT_ROOT(nvhash, struct hash);
POBJ_LAYOUT_TOID(nvhash, struct hash_node);
POBJ_LAYOUT_END(nvhash);

struct hash_node {
    TOID(struct string) keys[BUCKET_SLOTS];
    TOID(struct string) values[BUCKET_SLOTS];
    TOID(struct hash_node) next;
    int full;
};

struct hash {
    size_t size;
    TOID(struct hash_node) table;
};


// size: size of value to be stored
// NULL is returned if failed
TOID(struct hash)
new_hash(PMEMobjpool *pop, size_t size);

// 1 for success, -1 for failure
int
hash_insert(PMEMobjpool *pop,
            TOID(struct hash) h, const char *key, const char *value);

// NULL if failed
const char *
hash_get_value(const TOID(struct hash) h, const char *key);

// 1 for success, -1 for failure
int
hash_delete_item(PMEMobjpool *pop, TOID(struct hash) h, const char *key);

// 1 for success, -1 for failure
int
hash_remove_item(PMEMobjpool *pop, TOID(struct hash) h, const char *key);

void
hash_destroy(TOID(struct hash) h);

void
serialize_hash(TOID(struct hash) h);

void
hash_print(const TOID(struct hash) h);

#endif
