#define _GNU_SOURCE
#include <sched.h>
#include <assert.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define STACK_SIZE (4 * 1000 * 1000)

const char *host_name = "test-host";

static int img_entry()
{
    char *mnt_point = "/proc";
    mkdir(mnt_point, 0555);
    mount("proc", mnt_point, "proc", 0, NULL); // != -1

    sethostname(host_name, strlen(host_name));

    char *const args[] = {"/bin/bash", NULL};
    if (execvp(args[0], args) != 0)
    {
        fprintf(stderr, "failed to execvp arguments %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return 0;
}

int main(int argc, char **argv)
{
    char *stack;
    char *child_stack;
    int flags;
    pid_t pid;

    // MAP_STACK
    stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED)
    {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    child_stack = stack + STACK_SIZE;
    // CLONE_NEWUSER 应该还不支持
    flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWUTS | SIGCHLD;
    
    pid = clone(img_entry, child_stack, flags, NULL);
    if (pid < 0)
    {
        fprintf(stderr, "clone failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (waitpid(pid, NULL, 0) == -1)
    {
        fprintf(stderr, "failed to wait pid %d\n", pid);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}