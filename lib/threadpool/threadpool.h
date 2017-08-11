#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

struct threadpool;

struct threadpool_task;

struct threadpool_task *threadpool_task_create(void (*work)(struct threadpool_task *task, void *arg), void *arg);

void threadpool_task_release(struct threadpool_task *task);

struct threadpool *threadpool_create(int size);

void threadpool_release(struct threadpool *pool);

void threadpool_submit(struct threadpool *pool, struct threadpool_task *task);

int threadpool_cancel(struct threadpool *pool, struct threadpool_task *task);

#endif