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
    closure_ref int i = start;

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


// 这是一段有问题的代码, block2() 实际上返回了栈上的地址
typedef closure(Block, int, (void));
Block blockMaker()
{
    int a = 3;
    closure_ref Block block = lambda(int, (void) {
        return a;
    });
    return block;
}
int memory_test()
{
    int stack_var = 1;
    printf("stack %p\n", &stack_var);
    Block block2 = blockMaker();
    int b = block2();
    printf("%d ptr %p", b, &b);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////

#include "closure.h"

void closure_ref_test()
{
    int staticCounter = 1;
    closure_ref int refCounter = 1;

    closure(f, void, (void)) = lambda(void, (void) {
        printf("static %d ref %d\n", staticCounter, refCounter);
        refCounter++;
        // 未加 __block的修饰符的变量, 在 block内部引用, 是复制的语义, 一旦修改会报错
        // staticCounter++;
    });

    ++staticCounter;
    ++refCounter;

    f();

    printf("static %d ref %d\n", staticCounter, refCounter);

    // clang output: 
    //      static 1 ref 2
    //      static 2 ref 3
    // gcc output: 
    //      static 2 ref 2
    //      static 2 ref 3
    
    // 结论: 需要在lambda内部引用的upvalue 都加上 closure_ref 保证 clang 与 gcc 行为一致
}


/////////////////////////////////////////////////////////////////////////////////////

int main(int argc, const char *argv[])
{
    // upvalue_test();
    // test1();
    // counter_test();
    // memory_test();
    closure_ref_test();
    return 0;
}