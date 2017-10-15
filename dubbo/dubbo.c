#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h> // atoi
#include <stdarg.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include "codec.h"
#include "../base/cJSON.h"
#include "../base/dbg.h"
#include "../net/socket.h"
#include "../base/buffer.h"

extern char *optarg;
static const char *optString = "h:p:m:a:e:t:?";

struct dubbo_args
{
    char *host;
    char *port;
    char *service;
    char *method;
    char *args;   /* JSON */
    char *attach; /* JSON */
    struct timeval timeout;
};

#define ASSERT_OPT(assert, reason, ...)                                  \
    if (!(assert))                                                       \
    {                                                                    \
        fprintf(stderr, "\x1B[1;31m" reason "\x1B[0m\n", ##__VA_ARGS__); \
        usage();                                                         \
    }

static void usage()
{
    static const char *usage =
        "\nUsage:\n"
        "   dubbo -h<HOST> -p<PORT> -m<METHOD> -a<JSON_ARGUMENTS> [-e<JSON_ATTACHMENT='{}'> -t<TIMEOUT_SEC=5>]\n"
        "   dubbo -h<HOST> -p<PORT> -s [-t<TIMEOUT_SEC=5>] doc: https://github.com/youzan/zan/issues/18 \n\n"
        "Example:\n"
        "   dubbo -h127.0.0.1 -p8050 -s\n"
        "   dubbo -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.TokenService.getToken -a='{\"xxxId\":1,\"scope\":\"\"}'\n"
        "   dubbo -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.TokenService.getToken -a='{\"xxxId\":1,\"scope\":\"\"}' -e='{\"xxxId\":1}'\n"
        "   dubbo -h127.0.0.1 -p8050 -m=com.youzan.material.general.service.MediaService.getMediaList -a='{\"query\":{\"categoryId\":2,\"xxxId\":1,\"pageNo\":1,\"pageSize\":5}}'\n"
        "   dubbo -hqabb-dev-scrm-test0 -p8100 -mcom.youzan.scrm.customer.service.customerService.getByYzUid -a '{\"xxxId\":1, \"yzUid\": 1}'\n";
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

static bool dubbo_invoke(struct dubbo_args *args)
{
    bool ret = false;
    struct dubbo_res *res = NULL;

    struct dubbo_req *req = dubbo_req_create(args->service, args->method, args->args, args->attach);
    if (req == NULL)
    {
        return false;
    }
    struct buffer *buf = dubbo_encode(req);

    int sockfd = socket_clientSync(args->host, args->port);
    if (sockfd == -1)
    {
        goto release;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &args->timeout, sizeof(struct timeval));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &args->timeout, sizeof(struct timeval));

    size_t send_n = socket_sendAllSync(sockfd, buf_peek(buf), buf_readable(buf));
    if (send_n != buf_readable(buf))
    {
        goto release;
    }

    // fucking swoole, reactor 对EPOLLRDHUP事件直接当error处理，关闭, 不能正确处理版关闭事件
    // socket_shutdownWrite(sockfd);

    // reset buffer
    buf_retrieveAll(buf);

    int errno_ = 0;
    for (;;)
    {
        ssize_t recv_n = buf_readFd(buf, sockfd, &errno_);
        if (recv_n < 0)
        {
            if (errno_ == EINTR)
            {
                continue;
            }
            else
            {
                perror("ERROR readv");
                goto release;
            }
        }
        else if (!is_dubbo_pkt(buf))
        {
            goto release;
        }
        break;
    }

    int remaining = 0;
    if (!is_completed_dubbo_pkt(buf, &remaining))
    {
        goto release;
    }

    while (remaining < 0)
    {
        ssize_t recv_n = buf_readFd(buf, sockfd, &errno_);
        if (recv_n < 0)
        {
            if (errno_ == EINTR)
            {
                continue;
            }
            else
            {
                perror("ERROR receiving");
                goto release;
            }
        }
        remaining += recv_n;
    }

    res = dubbo_decode(buf);
    if (res == NULL)
    {
        goto release;
    }

    ret = true;

    if (res->is_evt)
    {
        puts("Recv dubbo evt pkt");
        goto release;
    }

    if (res->data_sz)
    {
        if (res->data[0] == '[' || res->data[0] == '{')
        {
            // char* json = malloc(res->data_sz + 1);
            // memcpy(json, res->data, res->data_sz);
            // json[res->data_sz] = '\0';
            // cJSON *resp = cJSON_Parse(json);
            cJSON *resp = cJSON_Parse(res->data);
            if (resp)
            {
                if (res->ok)
                {
                    printf("\x1B[1;32m%s\x1B[0m\n", cJSON_Print(resp));
                }
                else
                {
                    puts(res->desc);
                    printf("\x1B[1;31m%s\x1B[0m\n", cJSON_Print(resp));
                }
                cJSON_Delete(resp);
            }
            else
            {
                printf("\x1B[1;31m%s\x1B[0m\n", res->data);
            }
            // free(json);
        }
        else
        {
            printf("\x1B[1;32m%s\x1B[0m\n", res->data);
        }
    }

// fimxe print attach

release:
    dubbo_req_release(req);
    buf_release(buf);
    if (res)
    {
        dubbo_res_release(res);
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

    struct dubbo_args args;
    memset(&args, 0, sizeof(args));
    args.attach = "{}";
    args.timeout.tv_sec = 5;
    args.timeout.tv_usec = 0;

    opt = getopt(argc, argv, optString);
    optarg = trim_opt(optarg);
    while (opt != -1)
    {
        switch (opt)
        {
        case 'h':
            args.host = optarg;
            break;
        case 'p':
            args.port = optarg;
            break;
        case 'm':
        {
            char *c = strrchr(optarg, '.');
            ASSERT_OPT(c, "Invalid method %s", optarg);
            *c = 0;
            args.service = optarg;
            args.method = c + 1;
        }
        break;
        case 'a':
            args.args = optarg;
            break;
        case 'e':
            args.attach = optarg;
            break;
        case 't':
            args.timeout.tv_sec = atoi(optarg) > 0 ? atoi(optarg) : 5;
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

    ASSERT_OPT(args.host, "Missing Host -h=${host}");
    ASSERT_OPT(args.port, "Missing Port -p=${port}");
    ASSERT_OPT(args.service, "Missing Service -m=${service}.${method}");
    ASSERT_OPT(args.method, "Missing Method -m=${service}.${method}");
    ASSERT_OPT(args.args, "Missing Arguments -a'${jsonargs}'");

    cJSON *json_args = cJSON_Parse(args.args);
    ASSERT_OPT(json_args && cJSON_IsObject(json_args), "Invalid Arguments JSON Format : %s", args.args);
    cJSON_Delete(json_args);

    cJSON *json_attach = cJSON_Parse(args.attach);
    ASSERT_OPT(json_attach && cJSON_IsObject(json_attach), "Invalid Attach JSON Format as %s", args.attach);
    cJSON_Delete(json_attach);

    fprintf(stderr, "Invoking dubbo://%s:%s/%s.%s?args=%s&attach=%s\n",
            args.host, args.port, args.service, args.method, args.args, args.attach);

    return dubbo_invoke(&args) ? 0 : 1;
}