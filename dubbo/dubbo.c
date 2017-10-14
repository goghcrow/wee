#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "codec.h"
#include "../base/cJSON.h"
#include "../base/dbg.h"

int main(void)
{

    char *service = "com.x.y";
    char *method = "method";
    char *json_args = "[1,2,3,4]";
    char *json_attach = "";
    struct dubbo_req *req = dubbo_req_create(service, method, json_args, json_attach);
    if (req == NULL)
    {
        return 1;
    }

    struct buffer *buf = dubbo_encode(req);

    size_t sz = buf_readable(buf);
    
    puts("");
    printf("write sz: %zu\n", sz);
    
    char str[sz];
    memset(&str, 0, sizeof(str));
    buf_retrieveAsString(buf, sz, str);

    bin2p(str, sz);

    return 0;
}