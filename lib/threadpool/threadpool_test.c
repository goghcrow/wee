#include <stdio.h>
#include <unistd.h>
#include "threadpool.h"

// 释放task内存和任务取消之间有点冲突
// 先释放 再取消 ?!!!!!

static void do_work(struct threadpool_task *task, void *arg)
{
    // !!!!!
    // threadpool_task_release(task);
    sleep(1);
    puts("*");
    threadpool_task_release(task);
}

int main(int argc, char **argv)
{
    struct threadpool *pool;
    pool = threadpool_create(2);

    while (1)
    {
        struct threadpool_task *task1 = threadpool_task_create(do_work, NULL);
        struct threadpool_task *task2 = threadpool_task_create(do_work, NULL);
        struct threadpool_task *task3 = threadpool_task_create(do_work, NULL);

        threadpool_submit(pool, task1);
        threadpool_submit(pool, task2);
        threadpool_submit(pool, task3);
        sleep(1);
    }

    sleep(5);
    threadpool_release(pool);

    return 0;
}