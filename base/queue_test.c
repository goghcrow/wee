#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "queue.h"

struct Person
{
    int age;
    char *name;
    QUEUE node;
};

struct Person *newPerson(int age, char *name)
{
    struct Person *person;
    person = malloc(sizeof(struct Person));
    assert(person);
    memset(person, 0, sizeof(struct Person));
    person->name = name;
    person->age = age;
    return person;
}

void deletePerson(struct Person *person)
{
    free(person);
}

int main(int argc, char **argv)
{
    QUEUE q;
    QUEUE *iq;
    struct Person *ip;

    struct Person *p1;
    struct Person *p2;
    struct Person *p3;
    struct Person *p4;
    struct Person *p5;

    p1 = newPerson(1, "p1");
    p2 = newPerson(2, "p2");
    p3 = newPerson(3, "p3");
    p4 = newPerson(4, "p4");
    p5 = newPerson(5, "p5");

    QUEUE_INIT(&q);

    // QUEUE_INSERT_HEAD(&q, &p1->node);
    // QUEUE_INSERT_HEAD(&q, &p2->node);
    // QUEUE_INSERT_HEAD(&q, &p3->node);

    QUEUE_INSERT_TAIL(&q, &p1->node);
    QUEUE_INSERT_TAIL(&q, &p2->node);
    QUEUE_INSERT_TAIL(&q, &p3->node);
    QUEUE_INSERT_TAIL(&q, &p4->node);
    QUEUE_INSERT_TAIL(&q, &p5->node);

    // QUEUE_FOREACH(iq, &q) {
    //     ip = QUEUE_DATA(iq, struct Person, node);
    //     puts(ip->name);
    // }

    printf("%d\n", QUEUE_COUNT(&q));

    while(!QUEUE_EMPTY(&q)) {
        iq = QUEUE_HEAD(&q);
        ip = QUEUE_DATA(iq, struct Person, node);
        puts(ip->name);
        QUEUE_REMOVE(iq);
        deletePerson(ip);
    }

    printf("%d\n", QUEUE_COUNT(&q));
    

    /// 块设备 字符设备
    // while(1) {
    //     fprintf(stdout, "1");
    //     fprintf(stderr, "2");
    //     sleep(1);
    // }

    return 0;
}