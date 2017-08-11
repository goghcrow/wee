#ifndef MQ_H
#define MQ_H

#include <stdint.h>

struct mq;

struct msg
{
    void *ud;
    size_t sz;
};

struct mq *mq_create(int cap);

void mq_release(struct mq *q);

int mq_count(struct mq *q);

void mq_push(struct mq *q, struct msg *msg);

int mq_pop(struct mq *q, struct msg *msg);

// void mq_dump(struct mq *q);

#endif