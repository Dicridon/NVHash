#ifndef __FROST_NV_HASH__
#define __FROST_NV_HASH__
#include <libpmemobj.h>
#include "nvhash_dev.h"

typedef struct nvhash {
    TOID(struct hash) __hash__;
} nvhash_t;

nvhash_t *
new_nvhash(PMEMobjpool *pop, size_t size);

// 1 for success, -1 for failure
int
nvhash_insert(PMEMobjpool *pop,
            nvhash_t *h, const char *key, const char *value);

// NULL if failed
const char *
nvhash_get(const nvhash_t *t, const char *key);

// 1 for success, -1 for failure
// int
// nvhash_delete_item(PMEMobjpool *pop, nvhash_t* h, const char *key);

// 1 for success, -1 for failure
int
nvhash_remove(PMEMobjpool *pop, nvhash_t* h, const char *key);

void
nvhash_destroy(nvhash_t* h);

void
nvhash_print(nvhash_t* h);
#endif
