#ifndef CHAN_H
#define CHAN_H

#include <stdbool.h>
#include "mq.h"

struct chan;

/* cap=0 无限制容量 */
struct chan *ch_create(int cap);
void ch_release(struct chan *);
void ch_send(struct chan *, struct msg *);
bool ch_recv(struct chan *, struct msg *);

#endif