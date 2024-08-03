#include "co_routine.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <stack>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#ifdef __FreeBSD__
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#endif
using namespace std;
struct task_t
{
    stCoRoutine_t *co;
    int fd;
};
static stack<task_t *> g_readwrite;
static int g_listen_fd = -1;
static int SetNonBlock(int iSock)
{
    int iFlags;
    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;
}
static void *readwrite_routine(void *arg)
{
    co_enable_hook_sys();
    task_t *co = (task_t *)arg;
    char buf[1024 * 16];
    for (;;)
    {
        if (-1 == co->fd)
        {
            // push进去
            g_readwrite.push(co);
            // 切出
            co_yield_ct();
            continue;
        }
        int fd = co->fd;
        co->fd = -1;
        for (;;)
        {
            // 将该fd的可读事件，注册到epoll中
            struct pollfd pf = {0};
            pf.fd = fd;
            pf.events = (POLLIN | POLLERR | POLLHUP);

            co_poll(co_get_epoll_ct(), &pf, 1, 1000);

            // 当超时或者可读事件到达时，进⾏read。所以read不⼀定成功，有可能是超时造成的
            int ret = read(fd, buf, sizeof(buf));
            if (ret > 0)
            {
                // 构建HTTP响应
                const char *response = "HTTP/1.1 200 OK\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Content-Length: 13\r\n"
                                       "\r\n"
                                       "Hello, World!";

                // 发送HTTP响应
                ret = write(fd, response, strlen(response));
                printf("sent data, fd = %d, ret = %d\n", fd, ret);
            }
            if (ret <= 0)
            {
                if (errno == EAGAIN)
                    continue;
                close(fd);
                break;
            }
        }
    }
    return 0;
}
int co_accept(int fd, struct sockaddr *addr, socklen_t *len);
static void *accept_routine(void *)
{
    co_enable_hook_sys();
    printf("accept_routine\n");
    fflush(stdout);
    for (;;)
    {
        if (g_readwrite.empty())
        {
            printf("empty\n"); // sleep
            struct pollfd pf = {0};
            pf.fd = -1;
            // sleep 1秒，等待有空余的协程
            poll(&pf, 1, 1000);
            continue;
        }
        struct sockaddr_in addr; // maybe sockaddr_un;
        memset(&addr, 0, sizeof(addr));
        socklen_t len = sizeof(addr);
        // accept
        int fd = co_accept(g_listen_fd, (struct sockaddr *)&addr, &len);
        if (fd < 0)
        {
            // 意思是，如果accept失败了，没办法，暂时切出去
            struct pollfd pf = {0};
            pf.fd = g_listen_fd;
            pf.events = (POLLIN | POLLERR | POLLHUP);
            co_poll(co_get_epoll_ct(), &pf, 1, 1000);
            continue;
        }
        if (g_readwrite.empty())
        {
            close(fd);
            continue;
        }
        // 这⾥需要⼿动将其变成⾮阻塞的
        SetNonBlock(fd);
        task_t *co = g_readwrite.top();
        co->fd = fd;
        g_readwrite.pop();
        co_resume(co->co);
    }
    return 0;
}
static void SetAddr(const char *pszIP, const unsigned short shPort, struct sockaddr_in &addr)
{
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(shPort);
    int nIP = 0;
    if (!pszIP || '\0' == *pszIP || 0 == strcmp(pszIP, "0") || 0 == strcmp(pszIP, "0.0.0.0") || 0 == strcmp(pszIP, "*"))
    {
        nIP = htonl(INADDR_ANY);
    }
    else
    {
        nIP = inet_addr(pszIP);
    }
    addr.sin_addr.s_addr = nIP;
}
static int CreateTcpSocket(const unsigned short shPort /* = 0 */, const char *pszIP /* =
 "*" */, bool bReuse /* = false */)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd >= 0)
    {
        if (shPort != 0)
        {
            if (bReuse)
            {
                int nReuseAddr = 1;
                setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &nReuseAddr, sizeof(nReuseAddr));
            }
            struct sockaddr_in addr;
            SetAddr(pszIP, shPort, addr);
            int ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
            if (ret != 0)
            {
                close(fd);
                return -1;
            }
        }
    }
    return fd;
}
int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        printf("Usage:\n"
               "example_echosvr [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT]\n"
               "example_echosvr [IP] [PORT] [TASK_COUNT] [PROCESS_COUNT] -d #daemonize mode\n");
        return -1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int cnt = atoi(argv[3]);     // task_count 协程数
    int proccnt = atoi(argv[4]); // 进程数
    bool deamonize = argc >= 6 && strcmp(argv[5], "-d") == 0;
    g_listen_fd = CreateTcpSocket(port, ip, true);
    listen(g_listen_fd, 1024);
    if (g_listen_fd == -1)
    {
        printf("Port %d is in use\n", port);
        return -1;
    }
    printf("listen %d %s:%d\n", g_listen_fd, ip, port);
    SetNonBlock(g_listen_fd);
    for (int k = 0; k < proccnt; k++)
    {
        pid_t pid = fork();
        if (pid > 0)
        {
            continue;
        }
        else if (pid < 0)
        {
            break;
        }
        for (int i = 0; i < cnt; i++)
        {
            task_t *task = (task_t *)calloc(1, sizeof(task_t));
            task->fd = -1;
            // 创建⼀个协程
            co_create(&(task->co), NULL, readwrite_routine, task);
            // 启动协程
            co_resume(task->co);
        }
        // 启动listen协程
        stCoRoutine_t *accept_co = NULL;
        co_create(&accept_co, NULL, accept_routine, 0);
        // 启动协程
        co_resume(accept_co);
        // 启动事件循环
        co_eventloop(co_get_epoll_ct(), 0, 0);
        exit(0);
    }
    if (!deamonize)
        wait(NULL);
    return 0;
}
