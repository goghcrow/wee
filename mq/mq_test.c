#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "mq.h"

#define INIT_CAP 1

void test2()
{
    int i;
    struct msg msg;
    int ret;

    struct mq *q = mq_create(INIT_CAP);
    for (i = 0; i < 100; i++)
    {
        int n;
        msg.ud = malloc(10);
        // memset(msg->ud, 0, 10);
        msg.sz = 10;

        n = sprintf(msg.ud, "msg-%d", i);
        ((char *)msg.ud)[n] = 0;

        // printf("%s\n", msg->ud);
        mq_push(q, &msg);
    }

    // mq_dump(q);

    for (i = 0; i < 100; i++)
    {
        ret = mq_pop(q, &msg);
        assert(ret == 0);
        printf("%s\t", msg.ud);

        free(msg.ud);
    }

    ret = mq_pop(q, &msg);
    assert(ret == 1);

    mq_release(q);
}

void test1()
{
    struct msg ret;
    struct msg m1 = {.ud = "hello"};
    struct msg m2 = {.ud = "world"};

    struct mq *q = mq_create(3);
    printf("count = %d\n", mq_count(q));

    mq_push(q, &m1);
    printf("count = %d\n", mq_count(q));

    mq_push(q, &m2);
    printf("count = %d\n", mq_count(q));

    mq_push(q, &m2);
    printf("count = %d\n", mq_count(q));

    mq_push(q, &m2);
    printf("count = %d\n", mq_count(q));

    mq_push(q, &m2);
    printf("count = %d\n", mq_count(q));

    mq_push(q, &m2);
    printf("count = %d\n", mq_count(q));

    mq_push(q, &m2);
    printf("count = %d\n", mq_count(q));

    // mq_dump(q);

    mq_pop(q, &ret);
    printf("pop = %s count = %d \n", ret.ud, mq_count(q));

    mq_pop(q, &ret);
    printf("pop = %s count = %d \n", ret.ud, mq_count(q));

    mq_pop(q, &ret);
    printf("pop = %s count = %d \n", ret.ud, mq_count(q));

    mq_pop(q, &ret);
    printf("pop = %s count = %d \n", ret.ud, mq_count(q));

    mq_pop(q, &ret);
    printf("pop = %s count = %d \n", ret.ud, mq_count(q));

    mq_pop(q, &ret);
    printf("pop = %s count = %d \n", ret.ud, mq_count(q));

    mq_pop(q, &ret);
    printf("pop = %s count = %d \n", ret.ud, mq_count(q));

    mq_release(q);
}

void test3()
{
    int cap = 10000000;
    int i;
    struct msg m;

    struct mq *q = mq_create(cap);

    clock_t s = clock();
    for (i = 0; i < cap; i++)
    {
        m.ud = &i;
        mq_push(q, &m);
    }

    printf("%d\n", mq_count(q));

    for (i = 0; i < cap; i++)
    {
        mq_pop(q, &m);
    }
    clock_t e = clock();

    double cost = (double)(e - s) / CLOCKS_PER_SEC;
    printf("Elasped %f seconed\n", cost);
    printf("per seconed %f msg\n", cap / cost);

    mq_release(q);
}

int main(void)
{
    test3();
    // test2();
    // test1();
    return 0;
}
