#ifndef ATOMIC_H
#define ATOMIC_H


#include <stdint.h>

typedef struct atomic_int32_t {
    int32_t value;
} atomic_int32;

typedef struct atomic_int64_t {
    int64_t value;
} atomic_int64;

void atomic_int32_get(atomic_int32 *a32) {
    // in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
    return __sync_val_compare_and_swap(&a32->value, 0, 0);
}
void atomic_int32_getAndAdd() {}
void atomic_int32_addAndGet() {}
void atomic_int32_incrementAndGet(){}
void atomic_int32_decrementAndGet() {}
void atomic_int32_add()
void atomic_int32_increment()
void atomic_int32_decrement()
void atomic_int32_getAndSet()

#endif