#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include "../base/cJSON.h"
#include "generic.h"
#include "novacodec.h"
#include "../net/socket.h"
#include "../base/buffer.h"

struct globalArgs_t
{
    const char *host;
    const char *port;
    const char *service;
    const char *method;
    const char *args;   /* JSON */
    const char *attach; /* JSON */
    struct timeval timeout;
} globalArgs;

static const char *optString = "h:p:m:a:e:t:?s!";

#define INVALID_OPT(reason, ...)                                     \
    fprintf(stderr, "\x1B[1;31m" reason "\x1B[0m\n", ##__VA_ARGS__); \
    usage();

static void usage()
{
    static const char *usage =
        "\nUsage:\n"
        "   nova -h<HOST> -p<PORT> -m<METHOD> -a<JSON_ARGUMENTS> [-e<JSON_ATTACHMENT='{}'> -t<TIMEOUT_SEC=5>]\n"
        "   nova -h<HOST> -p<PORT> -s [-t<TIMEOUT_SEC=5>] doc: https://github.com/youzan/zan/issues/18 \n\n"
        "Example:\n"
        "   nova -h127.0.0.1 -p8050 -s\n"
        "   nova -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.TokenService.getToken -a='{\"xxxId\":1,\"scope\":\"\"}'\n"
        "   nova -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.TokenService.getToken -a='{\"xxxId\":1,\"scope\":\"\"}' -e='{\"xxxId\":1}'\n"
        "   nova -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.MediaService.getMediaList -a='{\"query\":{\"categoryId\":2,\"xxxId\":1,\"pageNo\":1,\"pageSize\":5}}'\n"
        "   nova -hqabb-dev-scrm-test0 -p8100 -mcom.youzan.scrm.customer.service.customerService.getByYzUid -a '{\"xxxId\":1, \"yzUid\": 1}'\n";
    puts(usage);
    exit(1);
}

// remove prefix = sapce and remove suffix space
static char *trim_opt(char *opt)
{
    char *end;
    if (opt == 0)
    {
        return 0;
    }

    while (isspace((int)*opt) || *opt == '=')
        opt++;

    if (*opt == 0)
    {
        return opt;
    }

    end = opt + strlen(opt) - 1;
    while (end > opt && isspace((int)*end))
        end--;

    *(end + 1) = 0;

    return opt;
}

static int nova_invoke()
{
    int ret = 1;
    char *resp_json = NULL;
    struct nova_hdr *nova_hdr = create_nova_generic(globalArgs.attach);
    struct buffer *nova_buf = buf_create(1024);

    struct buffer *generic_buf = thrift_generic_pack(0,
                                                     globalArgs.service, strlen(globalArgs.service),
                                                     globalArgs.method, strlen(globalArgs.method),
                                                     globalArgs.args, strlen(globalArgs.args));
    nova_pack(nova_buf, nova_hdr, buf_peek(generic_buf), buf_readable(generic_buf));
    buf_release(generic_buf);

    int sockfd = socket_clientSync(globalArgs.host, globalArgs.port);
    if (sockfd == -1)
    {
        goto fail;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &globalArgs.timeout, sizeof(struct timeval));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &globalArgs.timeout, sizeof(struct timeval));

    size_t send_n = socket_sendAllSync(sockfd, buf_peek(nova_buf), buf_readable(nova_buf));
    if (send_n != buf_readable(nova_buf))
    {
        goto fail;
    }
    
    socket_shutdownWrite(sockfd);
    
    // reset buffer
    buf_retrieveAll(nova_buf);
    
    int errno_ = 0;
    for (;;)
    {
        ssize_t recv_n = buf_readFd(nova_buf, sockfd, &errno_);
        if (recv_n < 0)
        {
            if (errno_ == EINTR)
            {
                continue;
            }
            else
            {
                perror("ERROR readv");
                goto fail;
            }
        }
        else if (recv_n < 4)
        {
            goto fail;
        }
        break;
    }
    
    int32_t msg_size = buf_peekInt32(nova_buf);
    if (msg_size <= 0)
    {
        fprintf(stderr, "ERROR: Invalid nova packet size %zd\n", msg_size);
        goto fail;
    }
    else if (msg_size > 1024 * 1024 * 2)
    {
        fprintf(stderr, "ERROR: too large nova packet size %zd\n", msg_size);
        goto fail;
    }

    while (buf_readable(nova_buf) < msg_size)
    {
        ssize_t recv_n = buf_readFd(nova_buf, sockfd, &errno_);
        if (recv_n < 0)
        {
            if (errno_ == EINTR)
            {
                continue;
            }
            else
            {
                perror("ERROR receiving");
                goto fail;
            }
        }
    }

    if (!nova_detect(nova_buf))
    {
        fprintf(stderr, "ERROR, invalid nova packet\n");
        goto fail;
    }

    // reset nova_hdr
    nova_hdr_release(nova_hdr);
    nova_hdr = nova_hdr_create();

    if (!nova_unpack(nova_buf, nova_hdr))
    {
        fprintf(stderr, "ERROR, fail to unpcak nova packet header\n");
        goto fail;
    }

    if (!thrift_generic_unpack(nova_buf, &resp_json))
    {
        fprintf(stderr, "ERROR, fail to unpack thrift packet\n");
        goto fail;
    }

    // print json attach
    {
        nova_hdr->attach[nova_hdr->attach_len] = 0;
        if (strcmp(nova_hdr->attach, "{}") != 0)
        {
            cJSON *root = cJSON_Parse(nova_hdr->attach);
            if (root)
            {
                printf("Nova Attachment: %s\n", cJSON_PrintUnformatted(root));
                cJSON_Delete(root);
            }
        }
    }

    // print json resp
    {
        cJSON *resp;
        cJSON *root = cJSON_Parse(resp_json);
        if (root == NULL || !cJSON_IsObject(root))
        {
            fprintf(stderr, "\x1B[1;31m"
                            "Invalid JSON Response"
                            "\x1B[0m\n");
            printf("%s", resp_json);
            goto fail;
        }
        else if ((resp = cJSON_GetObjectItem(root, "error_response")))
        {
            printf("\x1B[1;31m%s\x1B[0m\n", cJSON_Print(resp));
        }
        else if ((resp = cJSON_GetObjectItem(root, "response")))
        {
            printf("\x1B[1;32m%s\x1B[0m\n", cJSON_Print(resp));
        }
        else
        {
            printf("%s\n", cJSON_Print(root));
        }

        cJSON_Delete(root);
    }
    ret = 0;

fail:
    nova_hdr_release(nova_hdr);
    buf_release(nova_buf);
    if (resp_json != NULL)
    {
        free(resp_json);
    }
    if (sockfd != -1)
    {
        close(sockfd);
    }
    return ret;
}

int main(int argc, char **argv)
{
    int opt = 0;

    char *ret = NULL;
    size_t method_len = 0;
    size_t service_len = 0;

    globalArgs.attach = "{}";
    globalArgs.timeout.tv_sec = 5;
    globalArgs.timeout.tv_usec = 0;

    opt = getopt(argc, argv, optString);
    optarg = trim_opt(optarg);
    while (opt != -1)
    {
        switch (opt)
        {
        case 's':
            if (globalArgs.host == NULL)
            {
                globalArgs.host = "127.0.0.1";
            }
            globalArgs.service = "com.youzan.service.test";
            globalArgs.method = "stats";
            globalArgs.args = "{}";
            break;

        case 'h':
            globalArgs.host = optarg;
            break;

        case 'p':
            globalArgs.port = optarg;
            break;

        case 'm':
            ret = strrchr(optarg, '.');
            if (ret == NULL)
            {
                INVALID_OPT("Invalid method %s", optarg);
            }

            service_len = ret - optarg;
            method_len = strlen(optarg) - service_len - 1;
            *ret = 0;
            globalArgs.service = malloc(service_len + 1);
            globalArgs.method = malloc(method_len + 1);
            memcpy((void *)globalArgs.service, optarg, service_len + 1);
            memcpy((void *)globalArgs.method, ret + 1, method_len + 1);
            break;

        case 'a':
            globalArgs.args = optarg;
            break;

        case 'e':
            globalArgs.attach = optarg;
            break;

        case 't':
            globalArgs.timeout.tv_sec = atoi(optarg) > 0 ? atoi(optarg) : 5;
            break;

        case '?':
            usage();
            break;

        default:
            break;
        }
        opt = getopt(argc, argv, optString);
        optarg = trim_opt(optarg);
    }

    if (globalArgs.host == NULL)
    {
        INVALID_OPT("Missing Host");
    }

    if (atoi(globalArgs.port) <= 0)
    {
        INVALID_OPT("Missing Port");
    }

    if (globalArgs.service == NULL)
    {
        INVALID_OPT("Missing Service -m=${service}.${method}");
    }

    if (globalArgs.method == NULL)
    {
        INVALID_OPT("Missing Method -m=${service}.${method}");
    }

    if (globalArgs.args == NULL)
    {
        INVALID_OPT("Missing Arguments -a'${jsonargs}'");
    }
    else
    {
        cJSON *root = cJSON_Parse(globalArgs.args);
        if (root == NULL)
        {
            INVALID_OPT("Invalid Arguments JSON Format : %s", globalArgs.args);
        }
        else if (!cJSON_IsObject(root))
        {
            INVALID_OPT("Invalid Arguments JSON Format : %s", globalArgs.args);
        }
        else
        {
            // 泛化调用参数为扁平KV结构, 非标量参数要二次打包
            cJSON *cur = root->child;
            while (cur)
            {
                if (cJSON_IsArray(cur) || cJSON_IsObject(cur))
                {
                    cJSON_ReplaceItemInObject(root, cur->string, cJSON_CreateString(cJSON_PrintUnformatted(cur)));
                }

                cur = cur->next;
            }

            globalArgs.args = cJSON_PrintUnformatted(root);
        }
    }

    if (globalArgs.attach != NULL)
    {
        cJSON *root = cJSON_Parse(globalArgs.attach);
        if (root == NULL)
        {
            INVALID_OPT("Invalid Attach JSON Format as %s", globalArgs.attach);
        }
        else if (!cJSON_IsObject(root))
        {
            INVALID_OPT("Invalid Attach JSON Format as %s", globalArgs.attach);
        }
    }

    fprintf(stderr, "invoking nova://%s:%s/%s.%s?args=%s&attach=%s\n",
            globalArgs.host,
            globalArgs.port,
            globalArgs.service,
            globalArgs.method,
            globalArgs.args,
            globalArgs.attach);

    return nova_invoke();
}
