#ifndef WAITGROUP_H
#define WAITGROUP_H

struct waitgroup;

struct waitgroup *wg_create(int n);
void wg_release(struct waitgroup *);
void wg_add(struct waitgroup *);
void wg_done(struct waitgroup *);
void wg_wait(struct waitgroup *);
int wg_count(struct waitgroup *);

#endif