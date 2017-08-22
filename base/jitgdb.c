#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>


static void crash_handler(int sig)
{
    int pid;

#ifdef linux
    char *gdb_array[] = {"gdb", "", NULL};
#else
    char *gdb_array[] = {"lldb", "", NULL};
#endif
    char pid_str[40];

/* gdb --pid lldb --attach-pid */
#ifdef linux
    sprintf(pid_str, "-p=%d%c", getpid(), '\0');
#else
    sprintf(pid_str, "-p %d%c", getpid(), '\0');
#endif

    gdb_array[1] = pid_str;

    pid = fork();

    if (pid < 0)
    {
        abort();
    }
    else if (pid)
    {
        while (1)
        {
            sleep(3);   /* Give gdb time to attach */
            /* _exit(); */ /* You can skip this line by telling gdb to return */
        }
    }
    else
    {
#ifdef linux
        execvp("gdb", gdb_array);
#else
        execvp("lldb", gdb_array);
#endif
    }
}

void register_jit_debugger()
{
    signal(SIGQUIT, crash_handler); /* Normal got from Ctrl-\ */
    signal(SIGILL, crash_handler);
    signal(SIGTRAP, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGBUS, crash_handler);
    signal(SIGSEGV, crash_handler); /* This is most common crash */
    signal(SIGSYS, crash_handler);
}

void debugger()
{
    raise(SIGSEGV);
}



/*
struct sigaction sigact = {0};
sigact.sa_handler = signal_jit_handler;

sigaddset(&sigact.sa_mask, SIGSEGV);
sigaction(SIGSEGV, &sigcat, (struct sigaction *)NULL);

char gdb[160];
sprintf(gdb, "gdb --pid %d", getpid());
system(gdb);
*/

/*
int main(int argc, char **argv)
{
    int a = 1;
    int b = 2;
    register_jit_debugger();
    debugger();
    return 0;
}
*/