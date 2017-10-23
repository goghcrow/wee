#include <stdio.h>
#include <string.h>
#include <stdlib.h> /*atoi*/
#include <stdarg.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/time.h>
#include <ctype.h> /*isspace*/
#include <inttypes.h>

#include "dubbo_client.h"
#include "../base/cJSON.h"
#include "../base/dbg.h"
#include "../ae/ae.h"

extern char *optarg;
static const char *optString = "h:p:m:a:e:t:c:n:?";

#define ASSERT_OPT(assert, reason, ...)                                  \
    if (!(assert))                                                       \
    {                                                                    \
        fprintf(stderr, "\x1B[1;31m" reason "\x1B[0m\n", ##__VA_ARGS__); \
        usage();                                                         \
    }

// fixme support attach
// telnel 10.9.191.10 20881    ls  / invoke
// ./dubbo_test -h 10.9.191.10 -p 20881 -mcom.youzan.trade.core.biz.HelloService.hello -a "{}"
// ./dubbo_test -h 10.9.191.10 -p 20881 -mcom.youzan.trade.core.service.main.pay.TradePayService.pay -a "{}"
static void
usage()
{
    static const char *usage =
        "\nUsage:\n"
        "   dubbo -h<HOST> -p<PORT> -m<METHOD> -a<JSON_ARGUMENTS> [-e<JSON_ATTACHMENT='{}'> -t<TIMEOUT_SEC=5> -c<CONCURRENCY> -n<REQUESTS>]\n\n"
        "Example:\n"
        "   dubbo -h127.0.0.1 -p20881 -mcom.youzan.trade.core.biz.HelloService.hello -a \"{}\"\n";
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

int main(int argc, char **argv)
{
    struct dubbo_async_args async_args;
    memset(&async_args, 0, sizeof(async_args));
    async_args.req_n = 0;
    async_args.pipe_n = 0;

    struct dubbo_args args;
    memset(&args, 0, sizeof(args));
    args.attach = "{}";
    args.timeout.tv_sec = 5;
    args.timeout.tv_usec = 0;

    int opt = 0;
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
            args.timeout.tv_sec = atoi(optarg);
            break;
        case 'c':
            async_args.pipe_n = atoi(optarg);
            break;
        case 'n':
            async_args.req_n = atoi(optarg);
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
    ASSERT_OPT(args.timeout.tv_sec > 0, "Timeout must be positive");

    cJSON *json_args = cJSON_Parse(args.args);
    ASSERT_OPT(json_args && cJSON_IsObject(json_args), "Invalid Arguments JSON Format : %s", args.args);
    cJSON_Delete(json_args);

    cJSON *json_attach = cJSON_Parse(args.attach);
    ASSERT_OPT(json_attach && cJSON_IsObject(json_attach), "Invalid Attach JSON Format as %s", args.attach);
    cJSON_Delete(json_attach);

    fprintf(stderr, "Invoking dubbo://%s:%s/%s.%s?args=%s&attach=%s\n", args.host, args.port, args.service, args.method, args.args, args.attach);

    if (async_args.req_n > 0 && async_args.pipe_n > 0)
    {
        async_args.el = aeCreateEventLoop(1024);
        if (dubbo_bench_async(&args, &async_args))
        {
            aeMain(async_args.el);
            aeDeleteEventLoop(async_args.el);
            return 0;
        }
        else
        {
            aeDeleteEventLoop(async_args.el);
            return 1;
        }
    }
    else
    {
        return dubbo_invoke_sync(&args) ? 0 : 1;
    }
}