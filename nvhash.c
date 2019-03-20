#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "hash.h"

#define NULL_HASH_NODE TOID_NULL(struct hash_node)
#define EMPTY (-(BUCKET_SLOTS - 1))
#define FULL (1)

#define min(a, b) (((a) > (b)) ? b : a)

#define __mix__(a, b, c) \
{\
    a -= b; a -= c; a ^= (c >> 13); \
    b -= c; b -= a; b ^= (a << 8);  \
    c -= a; c -= b; c ^= (b >> 13); \
    a -= b; a -= c; a ^= (c >> 12); \
    b -= c; b -= a; b ^= (a << 16);  \
    c -= a; c -= b; c ^= (b >> 5); \
    a -= b; a -= c; a ^= (c >> 3); \
    b -= c; b -= a; b ^= (a << 10);  \
    c -= a; c -= b; c ^= (b >> 15); \
}

unsigned
jenkins_hash(const unsigned char *k, unsigned length, unsigned initval)
{
    unsigned a, b;
    unsigned c = initval;
    unsigned len = length;

    a = b = 0x9e3779b9;

    while(len >= 12) {
        a += k[0] + ((unsigned)k[1] << 8) + ((unsigned)k[2] << 16)
            + ((unsigned)k[3] << 24);
        b += k[4] + ((unsigned)k[5] << 8) + ((unsigned)k[6] << 16)
            + ((unsigned)k[7] << 24);
        c += k[8] + ((unsigned)k[9] << 8) + ((unsigned)k[10] << 16)
            + ((unsigned)k[11] << 24);

        __mix__(a, b, c);

        k += 12;
        len -= 12;
    }

    c += length;

    switch(len) {
    case 11: c += ((unsigned)k[10] << 24);
    case 10: c += ((unsigned)k[9] << 16);
    case 9: c += ((unsigned)k[8] << 8);
    case 8: b += ((unsigned)k[7] << 24);
    case 7: b += ((unsigned)k[6] << 16);
    case 6: b += ((unsigned)k[5] << 8);
    case 5: b += k[4];
    case 4: a += ((unsigned)k[3] << 24);
    case 3: a += ((unsigned)k[2] << 16);
    case 2: a += ((unsigned)k[1] << 8);
    case 1: a += k[0];
    }

    __mix__(a, b, c);
    return c;
}

int
hash_node_constr(PMEMobjpool* pop, void *ptr, void *arg)
{
    if (arg)
        return -1;
    struct hash_node *node_ptr = ptr;
    for (int i = 0; i < BUCKET_SLOTS; i++) {
        node_ptr->keys[i] = NULL_STR;
        node_ptr->values[i] = NULL_STR;
    }
    node_ptr->next = NULL_HASH_NODE;
    node_ptr->full = -(BUCKET_SLOTS - 1);
    pmemobj_persist(pop, node_ptr, sizeof(struct hash_node));
    return 1;
}

static TOID(struct hash_node)
new_node(PMEMobjpool *pop, const char *key, const char *value)
{
    TOID(struct hash_node) node = TOID_NULL(struct hash_node);
    POBJ_NEW(pop, &node, struct hash_node, hash_node_constr, NULL);
    if (TOID_IS_NULL(node)) {
        printf("No memory, %s\n", __FUNCTION__);
        return NULL_HASH_NODE;
    }

    struct hash_node *node_ptr = D_RW(node);
    str_new(pop, &node_ptr->keys[0], key);
    str_new(pop, &node_ptr->values[0], value);
    return node;
}

static int hash_constr(PMEMobjpool *pop, void *ptr, void *arg) {
    if (!arg)
        return -1;
    size_t size = *(size_t *)arg;
    struct hash_node *walk = ptr;
    for (size_t i = 0; i < size; i++) {
        for (int j = 0; j < BUCKET_SLOTS; j++) {
            walk->keys[i] = NULL_STR;
            walk->values[i] = NULL_STR;
            walk->next = NULL_HASH_NODE;
        }
    }
    pmemobj_persist(pop, ptr, size * sizeof(struct hash_node));
    return 1;
}

TOID(struct hash)
new_hash(PMEMobjpool *pop, size_t size)
{
    TOID(struct hash) hash = POBJ_ROOT(pop, struct hash);
    if (TOID_IS_NULL(hash)) {
        printf("No memory, %s\n", __FUNCTION__);
        assert(0);
    }

    struct hash *hash_ptr = D_RW(hash);

    
    hash_ptr->size = size;
    POBJ_ALLOC(pop,
               &hash_ptr->table,
               struct hash_node,
               size * sizeof(struct hash_node),
               hash_constr, &size);

    if(TOID_IS_NULL(hash_ptr->table)) {
        printf("No memory, %s\n", __FUNCTION__);
        POBJ_FREE(&hash);
        return TOID_NULL(struct hash);
    }
    pmemobj_persist(pop, hash_ptr, sizeof(struct hash));
    return hash;
}

int
insert(PMEMobjpool *pop, TOID(struct hash) h, const char *key, const char *value)
{
    struct hash *h_ptr = D_RW(h);
    size_t key_size = strlen(key);
    unsigned hash_key = jenkins_hash(key, key_size, *(unsigned*)key);
    hash_key %= h_ptr->size;

    struct hash_node *table = D_RW(h_ptr->table);
    
    struct hash_node *pre = NULL;
    struct hash_node *p = table + hash_key;

    printf("inserting %s=>%s at %ud\n",key,value,hash_key);

    while(!p && p->full == FULL) {
        pre = p;
        p = D_RW(p->next);
    }

    if (!p) {
        pre->next = new_node(pop, key, value);
        return TOID_IS_NULL(pre->next) ? -1 : 1;
    }

    int i = 0;
    for (i = 0; i < BUCKET_SLOTS; i++) {
        if (TOID_IS_NULL(p->keys[i]))
            break;
    }
    str_new(pop, &p->keys[i], key);
    str_new(pop, &p->values[i], value);
    p->full++;
    return 1;
}

const char *
get_value(TOID(struct hash) h, const char *key)
{
    struct hash *h_ptr = D_RW(h);
    size_t key_size = strlen(key);
    unsigned hash_key = jenkins_hash(key, key_size, *(unsigned*)key);
    hash_key %= h_ptr->size;
    struct hash_node *table = D_RW(h_ptr->table);
    struct hash_node *p = table + hash_key;

    while(p) {
        for (int i = 0; i < BUCKET_SLOTS; i++) {
            if (!TOID_IS_NULL(p->keys[i]) && strcmp(key,str_get(&p->keys[i])) == 0)
                return str_get(&p->values[i]);
            p = D_RW(p->next);
        }
    }
    return NULL;
}

// int
// delete_item(PMEMobjpool *pop,TOID(struct hash) h, const char *key)
// {
// 
// }

int
remove_item(PMEMobjpool *pop, TOID(struct hash) h, const char *key)
{
    struct hash *h_ptr = D_RW(h);
    
    size_t key_size = strlen(key);
    unsigned hash_key = jenkins_hash(key, key_size, *(unsigned*)key);
    hash_key %= h_ptr->size;
    struct hash_node *table = D_RW(h_ptr->table);
    struct hash_node *pre = table + hash_key;
    struct hash_node *cur = D_RW(pre->next);

    // the first node should not be removed
    for (int i = 0; i < BUCKET_SLOTS; i++) {
        if (!TOID_IS_NULL(pre->keys[i]) && strcmp(key,str_get(&pre->keys[i]))==0) {
            str_free(&pre->keys[i]);
            pre->keys[i] = NULL_STR;
            str_free(&pre->values[i]);
            pre->values[i] = NULL_STR;
            --pre->full;
            pmemobj_persist(pop, pre, sizeof(struct hash_node));
            return 1;
        }
    } 
    
    
    for(; cur; pre = cur, cur = D_RW(pre->next)) {
        for (int i = 0; i < BUCKET_SLOTS; i++) {
            if(!TOID_IS_NULL(cur->keys[i]) &&
               strcmp(str_get(&cur->keys[i]), key) == 0) {
                str_free(&pre->keys[i]);
                str_free(&pre->values[i]);
                cur->full--;
            }
            if (cur->full == EMPTY) {
                pre->next = cur->next;
                pmemobj_persist(pop, pre, sizeof(struct hash_node));
                PMEMoid oid = pmemobj_oid(cur);
                pmemobj_free(&oid);
                return 1;
            }
            cur->keys[i] = NULL_STR;
            cur->values[i] = NULL_STR;
            pmemobj_persist(pop, cur, sizeof(struct hash_node));
            return 1;
        }
    }
    return -1;
}

void
destroy_hash(PMEMobjpool *pop, TOID(struct hash) h)
{
    struct hash *h_ptr = D_RW(h);
    PMEMoid oid;
    for (size_t i = 0; i < h_ptr->size; i++) {
        struct hash_node *table = D_RW(h_ptr->table);
        struct hash_node *p = table + i;
        struct hash_node *q = D_RW(p->next);
        while(q) {
            for (int j = 0; j < BUCKET_SLOTS; j++) {
                str_free(&q->keys[i]);
                str_free(&q->values[i]);
            }
            p->next = q->next;
            oid = pmemobj_oid(q);
            pmemobj_free(&oid);
            q = D_RW(p->next);
        }
        for (int j = 0; j < BUCKET_SLOTS; j++) {
            str_free(&table->keys[i]);
            str_free(&table->values[i]);
        }
    }
    POBJ_FREE(&h_ptr->table);
}

void
print_hash(TOID(struct hash) h)
{
    struct hash *h_ptr = D_RW(h);
    struct hash_node *p = NULL;
    struct hash_node *table = D_RW(h_ptr->table);
    
    for (size_t i = 0; i < h_ptr->size; i++) {
        p = table + i;
        while(p) {
            for (int j = 0; j < BUCKET_SLOTS; j++)
                printf("%s=>%s, ", str_get(&p->keys[i]), str_get(&p->values[i]));
            p = D_RW(p->next);
        }
        printf("\n");
    }
}

