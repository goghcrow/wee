#ifndef COND_H
#define COND_H

// 一次性事件等待器, e.g. 用来处理项目启动顺序, a\b模块等待c模块初始化完成

struct waiter;

struct waiter *waiter_create();
void waiter_release(struct waiter *);
void waiter_signal(struct waiter *);
void waiter_wait(struct waiter *);

#endif