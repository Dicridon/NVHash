#include "nvhash.h"
#include <stdlib.h>
nvhash_t *
new_nvhash(PMEMobjpool *pop, size_t size)
{
    nvhash_t *rev = malloc(sizeof(nvhash_t));
    TOID(struct hash) h = new_hash(pop, size);
    rev->__hash__ = h;
    return rev;
}

// 1 for success, -1 for failure
int
nvhash_insert(PMEMobjpool *pop,
              nvhash_t *h, const char *key, const char *value)
{
    return hash_insert(pop, h->__hash__, key, value);
}

// NULL if failed
const char *
nvhash_get(const nvhash_t *t, const char *key)
{
    return hash_get_value(t->__hash__, key);
}

// 1 for success, -1 for failure
// int
// nvhash_delete_item(PMEMobjpool *pop, nvhash_t* h, const char *key)
// {
//     return hash_delete_item(pop, h->__hash__, key);
// }

// 1 for success, -1 for failure
int
nvhash_remove(PMEMobjpool *pop, nvhash_t* h, const char *key)
{
    return hash_remove_item(pop, h->__hash__, key);
}

void
nvhash_destroy(nvhash_t* h)
{
    hash_destroy(h->__hash__);
    free(h);
}

void
nvhash_print(nvhash_t *h)
{
    hash_print(h->__hash__);
}
