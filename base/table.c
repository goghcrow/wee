#include <stdlib.h>
#include "table.h"

/* 必须 pow of 2, 因为要-1所有bit变为1, &运算做hash */
#define INIT_SZ 16;
#define EXPAND_THRESHOLD 0.75

struct slot
{
    handle id;
    void *ud;
};

struct table
{
    handle last;
    int cap;
    int sz;
    struct slot *slots;
};

static inline int hash(struct table *t, int id)
{
    return id & (t->cap - 1);
}

struct table *table_create()
{
    struct table *t = malloc(sizeof(*t));
    if (t == NULL)
    {
        return t;
    }
    t->last = 0;
    t->sz = 0;
    t->cap = INIT_SZ;
    t->slots = (struct slot *)calloc(t->cap, sizeof(struct slot));
    if (t->slots == NULL)
    {
        free(t);
        return NULL;
    }
    return t;
}

void table_release(struct table *t)
{
    free(t->slots);
    free(t);
}

static struct table *table_expand(struct table *t)
{
    int ncap = t->cap * 2;
    struct slot *nslots = (struct slot *)calloc(ncap, sizeof(struct slot));
    if (nslots == NULL)
    {
        return NULL;
    }

    int i;
    for (i = 0; i < t->cap; i++)
    {
        nslots[i] = t->slots[i];
    }

    free(t->slots);
    t->slots = nslots;
    t->cap = ncap;
    return t;
}

handle table_set(struct table *t, void *ud)
{
    if (ud == NULL)
    {
        return 0;
    }

    if (t->sz >= t->cap * EXPAND_THRESHOLD)
    {
        if (table_expand(t) == NULL)
        {
            return 0;
        }
    }

    handle id;
    struct slot *s;
    for (;;)
    {
        id = ++t->last;
        if (id == 0)
        {
            id = ++t->last;
        }

        s = &t->slots[hash(t, id)];
        if (s->id != 0)
        {
            continue;
        }

        ++t->sz;
        s->id = id;
        s->ud = ud;
        return id;
    }
}

void *table_get(struct table *t, handle id)
{
    if (id == 0 || t->sz == 0)
    {
        return NULL;
    }

    struct slot *s = &t->slots[hash(t, id)];
    if (s->id != id)
    {
        return NULL;
    }
    else
    {
        return s->ud;
    }
}

void *table_del(struct table *t, handle id)
{
    if (id == 0 || t->sz == 0)
    {
        return NULL;
    }

    struct slot *s = &t->slots[hash(t, id)];
    void *ud;
    if (s->id != id)
    {
        return NULL;
    }
    else
    {
        s->id = 0;
        t->sz--;
        ud = s->ud;
        s->ud = NULL;
        return ud;
    }
}

int table_size(struct table *t)
{
    return t->sz;
}

int table_list(struct table *t, handle *ids, int sz)
{
    int i, j;
    for (i = 0, j = 0; i < t->cap && j < sz; i++)
    {
        if (t->slots[i].id == 0)
        {
            continue;
        }
        ids[j++] = t->slots[i].id;
    }

    return j;
}