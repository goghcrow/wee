#include <stdio.h>
#include <stdlib.h>

// brew install libedit
// sudo apt-get install libedit-dev
// su -c "yum install libedit-dev*"
// 将editline静态连接到prompt
// cc -std=c99 -Wall prompt.c -ledit -o prompt

// static char buf[1024];
// while (1)
// {
//     fputs("CLI> ", stdout);
//     fgets(buf, 1024, stdin);
//     printf("Input: %s", buf);
// }

#if 1
#include <string.h>
char *readline(const char *prompt)
{
    static char buf[2048];
    fputs(prompt, stdout);
    fgets(buf, 2048, stdin);
    char *p = malloc(strlen(buf) + 1);
    if (p == NULL)
    {
        return NULL;
    }
    strcpy(p, buf);
    p[strlen(p) - 1] = '\0';
    return p;
}

void add_history(const char *p)
{
}
#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

char *readline_(const char *prompt)
{
    static char *line = NULL;
    if (line)
    {
        free(line);
        line = NULL;
    }

    line = readline(prompt);
    if (line && *line)
    {
        add_history(line);
    }
    return line;
}

// #include <readline/readline.h>
// #include <readline/history.h>


int main(int argc, char **argv)
{
    puts("Version 0.0.1");
    puts("Please Ctrl+c to Exit\n");

    char *p;
    while ((p = readline_("CLI>")) != NULL)
    {
        add_history(p);
        printf("%s\n", p);
    }

    return 0;
}