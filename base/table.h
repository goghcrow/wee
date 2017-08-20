#ifndef TABLE_H
#define TABLE_H

typedef unsigned int handle;

struct table;

struct table *table_create();
void table_release(struct table *);

handle table_set(struct table *, void *ud);
void *table_get(struct table *, handle);
void *table_del(struct table *, handle);
int table_size(struct table *t);
/* 返回实际往ids填充handle数量 */
int table_list(struct table *t, handle *ids, int sz);

#endif