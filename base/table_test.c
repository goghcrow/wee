#include <stdio.h>
#include <assert.h>
#include "table.h"

void test_Table_get_set()
{
    struct table *t = table_create();

    int id1 = table_set(t, (void *)1);
    // printf("id1=%d\n", id1);
    assert(id1 != 0);
    assert((int)table_get(t, id1) == 1);

    int id2 = table_set(t, (void *)2);
    // printf("id2=%d\n", id2);
    assert(id2 != 0);
    assert((int)table_get(t, id2) == 2);

    table_release(t);
}

void test_Table_get_set2()
{
    struct table *t = table_create();

    // 填充至扩容
    handle ids[18];
    int i;
    for (i = 1; i < 18; i++)
    {
        ids[i] = table_set(t, (void *)i);
        assert(table_size(t) == i);
    }

    assert(table_size(t) == 17);

    for (i = 1; i < 18; i++)
    {
        assert((int)table_get(t, ids[i]) == i);
    }

    table_release(t);
}

void test_Table_del()
{
    struct table *t = table_create();

    int id1 = table_set(t, (void *)1);
    assert(id1 != 0);
    assert((int)table_get(t, id1) == 1);
    assert(table_size(t) == 1);

    void *ud = table_del(t, id1);
    assert(table_size(t) == 0);
    assert(table_get(t, id1) == NULL);
    assert((int)ud == 1);

    table_release(t);
}

void test_Table_list()
{
    struct table *t = table_create();

    // 填充至扩容
    handle ids[18];
    int i;
    for (i = 1; i < 18; i++)
    {
        ids[i] = table_set(t, (void *)i);
        assert(table_size(t) == i);
    }

    {
        int sz = 1;
        handle list[sz];
        int all = table_list(t, list, sz);
        assert(all == 1);
    }

    {
        int sz = table_size(t);
        handle list[sz];
        int all = table_list(t, list, sz);
        assert(all == 17);
        for (i = 0; i < all; i++)
        {
            table_del(t, list[i]);
        }
        assert(table_size(t) == 0);
    }

    table_release(t);
}

int main(void)
{
    test_Table_get_set();
    test_Table_get_set2();
    test_Table_del();
    test_Table_list();
    return 0;
}