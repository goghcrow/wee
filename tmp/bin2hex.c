#include <stdio.h>


static void die(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fputc('\n', stderr);
    exit(1);
}



static void printbin(const char *bin, int size)
{
    char c = '*';
    int count = 100;
    int i;
    for (i = 0; i < count; i++)
        putchar(c);
    puts("");
    fwrite(bin, sizeof(char), size, stdout);
    puts("");
    for (i = 0; i < count; i++)
        putchar(c);
    puts("");
}

#define DUMP_STRUCT(sp) bin2hex((const char *)(sp), sizeof(*(sp)))
#define DUMP_MEM(vp, n) bin2hex((const char *)(vp), (size_t)(n))
void bin2hex(const char *vp, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        printf("%02x", (unsigned char)vp[i]);
    }
    putchar('\n');
}