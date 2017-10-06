#ifndef MQ_H
#define MQ_H

#include <stdint.h>
#include <stdbool.h>

// 环形队列
struct mq;

struct msg
{
    void *ud;
    size_t sz;
};

struct mq *mq_create(int cap);
void mq_release(struct mq *);
int mq_count(struct mq *);
void mq_push(struct mq *, struct msg *);
bool mq_pop(struct mq *, struct msg *);
/* fixme  void mq_shrink(); */ 

#endif