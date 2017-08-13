#include <stdio.h>
#include "closure.h"

/////////////////////////////////////////////////////////////////////////////////////

void upvalue_test()
{
    int c = 10; // upvalue

    closure(add, int, (int, int)) = lambda(int, (int a, int b) {
        return a + b + c;
    });

    printf("%d\n", add(1, 2));
    // output: 133
}


/////////////////////////////////////////////////////////////////////////////////////
int binary_operator(int a, int b, closure(op, int, (int, int)))
{
    return op(a, b);
}

void test1()
{
    int ret;

    ret = binary_operator(1, 2, lambda(int, (int a, int b) {
        return a + b;
    }));
    printf("1+2=%d\n", ret);

    ret = binary_operator(1, 2, lambda(int, (int a, int b) {
        return a * b;
    }));
    printf("1*2=%d\n", ret);
}

/////////////////////////////////////////////////////////////////////////////////////

typedef closure(IntBlock, int, (void));

typedef struct
{
    IntBlock forward;
    IntBlock backward;
} Counter;

Counter MakeCounter(int start, int increment)
{
    Counter counter;

    // clang
    closure_var int i = start;

    counter.forward = closure_copy(lambda(int, (void) {
        i += increment;
        return i;
    }));

    counter.backward = closure_copy(lambda(int, (void) {
        i -= increment;
        return i;
    }));

    return counter;
}

void ReleaseCounter(Counter counter)
{
    closure_release(counter.forward);
    closure_release(counter.backward);
}

void counter_test()
{
    Counter counter = MakeCounter(5, 2);
    printf("Forward one: %d\n", counter.forward());
    printf("Forward one more: %d\n", counter.forward());
    printf("Backward one: %d\n", counter.backward());
    ReleaseCounter(counter);
}

/////////////////////////////////////////////////////////////////////////////////////

int main(int argc, const char *argv[])
{
    upvalue_test();
    test1();
    counter_test();
    return 0;
}