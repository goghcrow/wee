#include <stdbool.h>
#include <sys/socket.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dubbo_codec.h"
#include "dubbo_client.h"
#include "../net/socket.h"
#include "../base/buffer.h"
#include "../base/cJSON.h"

bool dubbo_invoke(struct dubbo_args *args)
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
            // 返回 json, 不应该有 NULL 存在, 且非 NULL 结尾
            char *json = malloc(res->data_sz + 1);
            assert(json);
            memcpy(json, res->data, res->data_sz);
            json[res->data_sz] = '\0';

            cJSON *resp = cJSON_Parse(json);
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
            free(json);
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
