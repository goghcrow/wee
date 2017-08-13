#include <stdio.h>
#include <stdlib.h>

typedef struct
{
} * Object;

Object new()
{
    Object obj = malloc(sizeof(*obj));
    return obj;
}

void ReleaseObject(Object obj)
{
    free(obj);
}

int main(int argc, const char *argv[])
{
    Object obj = new();

    return 0;
}