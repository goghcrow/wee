#ifndef WAITER_H
#define WAITER_H

struct waiter;

struct waiter *waiter_create();
void waiter_release(struct waiter *w);
void waiter_signal(struct waiter *w);
void waiter_wait(struct waiter *w);

#endif