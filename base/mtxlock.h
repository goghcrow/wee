#ifndef MTXLOCK_H
#define MTXLOCK_H

#include <pthread.h>
#include <stdbool.h>

struct mtxlock;

struct mtxlock *mtl_create();
void mtl_release(struct mtxlock *);
void mtl_lock(struct mtxlock *);
void mtl_unlock(struct mtxlock *);
bool mtl_lockedbyself(struct mtxlock *);
pthread_mutex_t *mtl_getmtx(struct mtxlock *);

#endif
