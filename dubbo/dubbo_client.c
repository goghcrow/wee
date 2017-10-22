#include <stdbool.h>
#include <sys/socket.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "../ae/ae.h"
#include "../ae/anet.h"

#include "dubbo_codec.h"
#include "dubbo_client.h"
#include "../net/socket.h"
#include "../base/buffer.h"
#include "../base/cJSON.h"
#include "../base/mq.h"
#include "../base/dbg.h"

#define CLI_INIT_BUF_SZ 1024

static char err[ANET_ERR_LEN];

static void cli_on_recv(struct aeEventLoop *el, int fd, void *ud, int mask);
static void cli_on_write(struct aeEventLoop *el, int fd, void *ud, int mask);
static void cli_decode_resp_frombuf(struct buffer *buf);

struct buffer_node
{
    struct buffer *buf;
    struct buffer *nxt;
};

struct dubbo_client
{
    struct aeEventLoop *el;
    struct dubbo_args *args;

    struct buffer *rcv_buf;
    struct buffer *snd_buf;
    int pipe_n;
    int pipe_left;

    int fd;
    bool connected;
};

static struct buffer *cli_encode_req(struct dubbo_client *cli)
{
    struct dubbo_req *req = dubbo_req_create(cli->args->service, cli->args->method, cli->args->args, cli->args->attach);
    if (req == NULL)
    {
        return false;
    }
    return dubbo_encode(req);
}

#define cli_decode_resp(cli) cli_decode_resp_frombuf(((struct dubbo_client *)(cli))->rcv_buf)

struct dubbo_client *cli_create(struct aeEventLoop *el, struct dubbo_args *args, int pipe_n)
{
    struct dubbo_client *cli = calloc(1, sizeof(*cli));
    assert(cli);
    cli->el = el;

    cli->rcv_buf = buf_create(CLI_INIT_BUF_SZ);
    cli->snd_buf = buf_create(CLI_INIT_BUF_SZ);
    cli->pipe_n = pipe_n;
    cli->pipe_left = pipe_n;
    cli->args = args;
    cli_reset(cli);
    return cli;
}

void cli_release(struct dubbo_client *cli)
{
    buf_release(cli->rcv_buf);
    buf_release(cli->snd_buf);
    free(cli);
}

static void cli_reset(struct dubbo_client *cli)
{
    cli->connected = false;
    cli->fd = -1;
    cli->pipe_left = cli->pipe_n;
    buf_retrieveAll(cli->rcv_buf);
    buf_retrieveAll(cli->snd_buf);
}

static void cli_close(struct dubbo_client *cli)
{
    aeDeleteFileEvent(cli->el, cli->fd, AE_READABLE | AE_WRITABLE);
    close(cli->fd);
    cli_reset(cli);
}

static bool cli_connect(struct dubbo_client *cli)
{
    int fd = anetTcpNonBlockConnect(err, cli->args->host, atoi(cli->args->port));
    if (fd == ANET_ERR)
    {
        return false;
    }
    anetEnableTcpNoDelay(err, fd);
    anetTcpKeepAlive(err, fd);
    cli->fd = fd;

    if (AE_ERR == aeCreateFileEvent(cli->el, fd, AE_READABLE, cli_on_recv, cli))
    {
        close(fd);
        return false;
    }
    return true;
}

static void cli_reconnect(struct dubbo_client *cli)
{
    cli_close(cli);
    if (!cli_connect(cli))
    {
        LOG_ERROR("重连失败");
    }
}

static bool cli_write(struct dubbo_client *cli)
{
    struct buffer *buf = cli->snd_buf;
    if (!buf_readable(buf))
    {
        aeDeleteFileEvent(cli->el, cli->fd, AE_WRITABLE);
        return true;
    }

    int nwritten = 0;
    while (buf_readable(buf))
    {
        nwritten = write(cli->fd, buf_peek(buf), buf_readable(buf));
        if (nwritten <= 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        buf_retrieve(buf, nwritten);
    }

    if (nwritten <= 0)
    {
        if (errno == EAGAIN)
        {
            if (AE_ERR == aeCreateFileEvent(cli->el, cli->fd, AE_WRITABLE, cli_on_write, cli))
            {
                LOG_ERROR("Dubbo 请求失败: 创建可写事件失败");
                return false;
            }
            return true;
        }
        else
        {
            LOG_ERROR("Dubbo 发送数据失败: %s", strerror(errno));
            return false;
        }
    }

    if (!buf_readable(buf))
    {
        aeDeleteFileEvent(cli->el, cli->fd, AE_WRITABLE);
    }
}

static void cli_send_req(struct dubbo_client *cli)
{
    buf_readable(cli->snd_buf);

    struct buffer *buf = cli_encode_req(cli);
    if (buf == NULL)
    {
        PANIC("Dubbo 请求失败: 编码失败");
        return;
    }

    buf_append(cli->snd_buf, buf_peek(buf), buf_readable(buf));
    buf_release(buf);
    if (!cli_write(cli))
    {
        cli_reconnect(cli);
    }
}

static void cli_on_write(struct aeEventLoop *el, int fd, void *ud, int mask)
{
    UNUSED(el);
    UNUSED(fd);
    UNUSED(mask);

    struct dubbo_client *cli = (struct dubbo_client *)ud;
    if (!cli_write(cli))
    {
        cli_reconnect(cli);
    }
}

static void cli_on_recv(struct aeEventLoop *el, int fd, void *ud, int mask)
{
    UNUSED(el);
    UNUSED(mask);

    struct dubbo_client *cli = (struct dubbo_client *)ud;
    if (cli->connected)
    {
        for (;;)
        {
            int errno_ = 0;
            ssize_t recv_n = buf_readFd(cli->rcv_buf, fd, &errno_);
            if (recv_n < 0)
            {
                if (errno_ == EINTR)
                {
                    continue;
                }
                else if (errno_ == EAGAIN)
                {
                }
                else
                {
                    LOG_ERROR("从 Dubbo 服务端读取数据: %s", strerror(errno));
                    cli_reconnect(cli);
                    return;
                }
            }
            else if (recv_n == 0)
            {
                LOG_ERROR("Dubbo 服务端断开连接");
                cli_reconnect(cli);
                return;
            }
            break;
        }

        if (!is_dubbo_pkt(cli->rcv_buf))
        {
            LOG_ERROR("接收到非 dubbo 数据包");
            cli_reconnect(cli);
        }
        if (is_completed_dubbo_pkt(cli->rcv_buf, NULL))
        {
            cli->pipe_left++;
            cli_decode_resp(cli);
            goto send;
        }
        return;
    }
    else
    {
        cli->connected = true;
    }

send:
    while (cli->pipe_left--)
    {
        cli_send_req(cli);
    }
}

static void cli_decode_resp_frombuf(struct buffer *buf)
{
    struct dubbo_res *res = dubbo_decode(buf);
    if (res == NULL)
    {
        return false;
    }

    if (res->is_evt)
    {
        LOG_INFO("接收到 dubbo 事件包");
    }
    else if (res->data_sz)
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
                    LOG_INFO("%s", cJSON_Print(resp));
                }
                else
                {
                    LOG_ERROR("%s", res->desc);
                    LOG_ERROR("%s", cJSON_Print(resp));
                }
                cJSON_Delete(resp);
            }
            else
            {
                LOG_INFO("%s", res->data);
            }
            free(json);
        }
        else
        {
            LOG_ERROR("%s", res->data);
        }
    }

    dubbo_res_release(res);
    return true;
}

bool dubbo_invoke_sync(struct dubbo_args *args)
{
    bool ret = false;
    struct dubbo_res *res = NULL;

    struct dubbo_req *req = dubbo_req_create(args->service, args->method, args->args, args->attach);
    if (req == NULL)
    {
        return false;
    }

    struct buffer *buf = dubbo_encode(req);
    if (!buf)
    {
        dubbo_req_release(req);
        return false;
    }

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

    cli_decode_resp_frombuf(buf);

// fimxe print attach

release:
    dubbo_req_release(req);
    buf_release(buf);
    if (sockfd != -1)
    {
        close(sockfd);
    }
    return ret;
}