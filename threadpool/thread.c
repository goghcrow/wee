#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>


struct thread {
    bool started;
    bool joined;
    pid_t tid;
    
};

int main(void)
{
    #ifdef __APPLE__
    pid_t pid = syscall(SYS_thread_selfid);
    #else
    pid_t pid = syscall(SYS_gettid);
    #endif

    printf("%d\n", pid);
    return 0;
}