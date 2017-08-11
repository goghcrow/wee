#include <stdio.h>
#include <stdlib.h>

struct node
{
    int data;
    struct node *next;
};

/// **  reference pointer

int length(struct node *head);
void push(struct node **headRef, int data);


static struct node *
createNode(int data)
{
    struct node *_node = calloc(1, sizeof(struct node));
    _node->data = data;
    return _node;
}

static void
releaseNode(struct node *_node)
{
    free(_node);
}

int length(struct node *head)
{
    int count = 0;
    struct node *current = head;

    while(current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

struct node *buildWithDummyNode()
{
    struct node dummy;
    struct node *tail = &dummy;
    int i;
    
    // !!!!! WHY ??????
    dummy.next = NULL;
    
    for (i = 1; i < 6; i++) {
        push(&(tail->next), i);
        tail = tail->next;
    }

    return dummy.next;
}

struct node *buildWithLocalReference()
{
    struct node *head = NULL;
    struct node **lastPtrRef = &head;

    int i = 1;
    for (i = 1; i < 6; i++) {
        push(lastPtrRef, i);
        lastPtrRef = &((*lastPtrRef)->next);
    }
    return head;
}


void listToString(struct node *head)
{
    struct node **lastPtrRef = &head;
    if (head == NULL) {
        printf("[]\n");
    } else {
        printf("[");
        while(*lastPtrRef) {
            printf(" %d,", (*lastPtrRef)->data);
            lastPtrRef = &((*lastPtrRef)->next);
        }
        printf("]\n");
    }
}

void push(struct node **headRef, int data)
{
    struct node *newHead = createNode(data);
    newHead->next = *headRef;
    *headRef = newHead;
}

void basicCaller()
{
    struct node *head;

    head = buildWithDummyNode();
    listToString(head);

    head = buildWithLocalReference();
    listToString(head);
    
    // push(&head, 13);
    // push(&(head->next), 42);


    printf("list length: %d\n", length(head));
    listToString(head);
}

int main(int argc, char **argv)
{
    struct node *head = NULL;
    basicCaller();

    return 0;
}