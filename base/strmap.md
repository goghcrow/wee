### Description

This is a simple hash table implementation in ANSI C. It supports the rudimentary functions generally expected of a hash table:

1. Inserting and retrieving key-value associations
2. Querying the existence of a key
3. Returning the total number of key-value associations
4. Iterating over all key-value associations

### Source code

The API is documented in the header file (strmap.h).

strmap.h

strmap.c

### License

The code ("strmap") is licensed under the GNU Lesser General Public License.

### Notes

First, the code is not thread-safe. Use external synchronization.

Second, the implementation copies values upon insertion and retrieval. This guarantees that no internal references are exposed to the client after the hash table has been deleted from memory. There is one exception, however. When iterating over all key-value associations, the internal strings are temporarily exposed as constant pointers to the internal string buffers. This is because it is very slow and cumbersome to copy all key-value associations to a buffer via a callback function interface. It is recommended not to keep the pointers to the internal string buffers around out of scope of the callback function. Otherwise, you risk having wild pointers in your code after you have deleted the hash table.

### Example usage

The code fragments below create a StrMap object, insert and retrieve a couple of string associations, and finally destroy the StrMap object.

```
#include "strmap.h"

...

StrMap *sm;
char buf[255];
int result;

sm = sm_new(10);
if (sm == NULL) {
    /* Handle allocation failure... */
}
/* Insert a couple of string associations */
sm_put(sm, "application name", "Test Application");
sm_put(sm, "application version", "1.0.0");
/* Retrieve a value */
result = sm_get(sm, "application name", buf, sizeof(buf));
if (result == 0) {
    /* Handle value not found... */
}
printf("value: %s\n", buf)

...

/* When done, destroy the StrMap object */
sm_delete(sm);

...
```

The code fragments below demonstrate how to iterate over all key-value associations:

```
#include "strmap.h"

...

StrMap *sm;

...

static void iter(const char *key, const char *value, const void *obj)
{
    printf("key: %s value: %s\n", key, value);
}

...

sm_enum(sm, iter, NULL);

...
```