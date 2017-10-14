#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../base/cJSON.h"

cJSON *rebuild_jsonargs(char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
    {
        return NULL;
    }
    if (!cJSON_IsArray(root) && !cJSON_IsObject(root))
    {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *obj = cJSON_CreateObject();
    int i = 0;
    char buf[10];
    memset(buf, 0, sizeof(buf));

    cJSON *el;
    cJSON_ArrayForEach(el, root)
    {
        sprintf(buf, "arg%d", i++);
        cJSON_AddItemToObject(obj, buf, cJSON_Duplicate(el, true));
    }

    cJSON_Delete(root);
    return obj;
}

int main(void)
{
    char *json_str_arr = "[42, [\"white\", \"black\"]]";
    char *json_str_obj = "{\"id\":42, \"color\":[\"white\", \"black\"]}";

    cJSON obj* = rebuild_jsonargs(json_str_arr);
    if (obj == NULL) {
        printf(stderr, "ERROR: invalid json %s\n", json_str_arr);
        return 1;
    }
    printf("%s\n", cJSON_Print(obj));
    cJSON_Delete(obj);

    cJSON obj* = rebuild_jsonargs(json_str_obj);
    if (obj == NULL) {
        printf(stderr, "ERROR: invalid json %s\n", json_str_obj);
        return 1;
    }
    printf("%s\n", cJSON_Print(obj));
    cJSON_Delete(obj);

    return 0;
}