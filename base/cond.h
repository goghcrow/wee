#ifndef COND_H
#define COND_H

#include "mtxlock.h"

struct cond;

struct cond *cond_create(struct mtxlock *);
void cond_release(struct cond *);
void cond_signal(struct cond *);
void cond_broadcast(struct cond *);
void cond_wait(struct cond *);
// return true when timeout
bool cond_timedwait(struct cond *, double sec);
struct mtxlock *cond_getlock(struct cond *);

#endif