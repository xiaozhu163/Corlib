#include "hook.h"
#include "ioscheduler.h"
#include <dlfcn.h> // 包含动态链接库函数，如dlsym
#include <iostream>
#include <cstdarg> // 包含可变参数宏，如va_list等
#include "fd_manager.h"
#include <string.h>

// 将所有函数应用到HOOK_FUN宏
#define HOOK_FUN(XX) \
    XX(sleep)        \
    XX(usleep)       \
    XX(nanosleep)    \
    XX(socket)       \
    XX(connect)      \
    XX(accept)       \
    XX(read)         \
    XX(readv)        \
    XX(recv)         \
    XX(recvfrom)     \
    XX(recvmsg)      \
    XX(write)        \
    XX(writev)       \
    XX(send)         \
    XX(sendto)       \
    XX(sendmsg)      \
    XX(close)        \
    XX(fcntl)        \
    XX(ioctl)        \
    XX(getsockopt)   \
    XX(setsockopt)

namespace corlib
{

    // 用于指示当前线程是否使用hook函数
    static thread_local bool t_hook_enable = false;

    // 检查hook是否启用
    bool is_hook_enable()
    {
        return t_hook_enable;
    }

    // 设置hook启用状态
    void set_hook_enable(bool flag)
    {
        t_hook_enable = flag;
    }

    // 初始化hook函数 给库函数定义了一堆的新函数指针，这些函数指针指向原始的库函数，然后在这个函数中通过dlsym函数获取原始库函数的地址，然后将这个地址赋值给新定义的函数指针
    void hook_init()
    {
        static bool is_inited = false; // 静态变量，确保只初始化一次
        if (is_inited)
        {
            return;
        }

        is_inited = true;

// 将原始函数地址分配给函数指针 -> 使用dlsym从动态链接库中获取函数符号
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
        HOOK_FUN(XX)
#undef XX
    }

    // 静态变量初始化，将在main函数之前运行
    struct HookIniter
    {
        HookIniter()
        {
            hook_init(); // 调用hook初始化函数
        }
    };

    static HookIniter s_hook_initer;

} // end namespace corlib

// 定义定时器信息结构体
struct timer_info
{
    int cancelled = 0; // 取消标志
};

/*
具体执行到 EAGAIN 处理部分的条件
文件描述符是套接字。
套接字设置为非阻塞模式。
系统调用返回 EAGAIN，表示资源暂时不可用。
只有在满足这些条件时，代码才会执行到 EAGAIN 处理部分，添加事件和定时器，并挂起当前协程。
*/

// 通用模板函数用于处理读写操作 (主要处理套接字操作，下面重写的套接字读写函数用这里的 代理模式 处理) 其他的sleep等库函数直接调用下面的重写函数
template <typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name, uint32_t event, int timeout_so, Args &&...args)
{
    // 如果hook未启用，直接调用原始函数
    if (!corlib::t_hook_enable)
    {
        return fun(fd, std::forward<Args>(args)...);
    }

    // 获取文件描述符控制上下文
    std::shared_ptr<corlib::FdCtx> ctx = corlib::FdMgr::GetInstance()->get(fd);
    if (!ctx)
    {
        return fun(fd, std::forward<Args>(args)...); // 如果获取失败，直接调用原始函数
    }

    // 如果文件描述符已关闭，返回错误
    if (ctx->isClosed())
    {
        errno = EBADF; // 错误码设置为EBADF
        return -1;
    }

    // 如果不是socket或已设置为用户非阻塞模式，直接调用原始函数
    if (!ctx->isSocket() || ctx->getUserNonblock())
    {
        return fun(fd, std::forward<Args>(args)...);
    }

    // 获取超时时间
    uint64_t timeout = ctx->getTimeout(timeout_so);    // timeout_so = SO_RCVTIMEO or SO_SNDTIMEO
    std::shared_ptr<timer_info> tinfo(new timer_info); // 创建定时器信息结构体

retry:
    // 调用原始函数
    ssize_t n = fun(fd, std::forward<Args>(args)...);

    // 如果操作被系统中断，重试
    while (n == -1 && errno == EINTR)
    {
        n = fun(fd, std::forward<Args>(args)...);
    }

    // 如果资源暂时不可用，等待直到准备就绪
    if (n == -1 && errno == EAGAIN)
    {
        corlib::IOManager *iom = corlib::IOManager::GetThis();
        std::shared_ptr<corlib::Timer> timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        // 如果设置了超时时间，添加条件定时器以取消操作
        if (timeout != (uint64_t)-1)
        {
            timer = iom->addConditionTimer(timeout, [winfo, fd, iom, event]() { // 定时器到了要执行的函数
                auto t = winfo.lock();
                if (!t || t->cancelled)
                {
                    return;
                }
                t->cancelled = ETIMEDOUT;                                // 设置取消标志
                iom->cancelEvent(fd, (corlib::IOManager::Event)(event)); // 取消事件
            },
                                           winfo);
        }

        // 添加事件，将当协程作为回调 ，也就是不添加回调函数，因为之前已经添加过fd对应的回调了，该套接字对应的文件描述符上的事件发生时，会唤醒当前协程
        int rt = iom->addEvent(fd, (corlib::IOManager::Event)(event)); // 这里添加事件（下面每个重写的函数都对应一个读/写事件，只要非阻塞读/写失败，先添加了定时器，然后就在这注册上事件了），事件会在发生时在idle函数中取消，或者在超时处理函数中取消
                                                                       // 之后要么正常读到数据在，要么超时执行定时器超时程序
        if (rt)
        {
            std::cout << hook_fun_name << " addEvent(" << fd << ", " << event << ")";
            if (timer)
            {
                timer->cancel();
            }
            return -1;
        }
        else
        {
            corlib::Fiber::GetThis()->yield(); // 添加定时器后，当前协程让出执行权 去执行其他任务，直到事件发生或者定时器超时，定时器超时执行定时器任务（在定时器里取消事件，取消事件时会执行到这里一次）
                                               // 或者正常执行（比如等待的数据到达）也会执行到这里，执行到这里后，会继续执行下面的代码

            // 恢复执行
            if (timer) // 如果定时器还没到时间就取消定时器
            {
                timer->cancel(); // 取消定时器
            }
            if (tinfo->cancelled == ETIMEDOUT) // 如果正常执行到这里是主动取消的定时器不会执行该条件，因为只有定时器超时执行超时函数才会置该标志位
            {
                errno = tinfo->cancelled;
                return -1; // 定时器超时后，取消事件时会执行一次事件函数，也就会执行到这里，定时器超时返回-1
            }
            goto retry; // 比如：数据到了去读数据
        }
    } // 正常执行完库函数退出；
    return n;
}

extern "C"
{

// 定义原始函数指针，初始化为nullptr
#define XX(name) name##_fun name##_f = nullptr;
    HOOK_FUN(XX)
#undef XX

    // 仅在任务协程中使用
    unsigned int sleep(unsigned int seconds)
    {
        if (!corlib::t_hook_enable)
        {
            return sleep_f(seconds);
        }

        std::shared_ptr<corlib::Fiber> fiber = corlib::Fiber::GetThis();
        corlib::IOManager *iom = corlib::IOManager::GetThis();
        // 添加定时器以重新调度此协程
        iom->addTimer(seconds * 1000, [fiber, iom]()
                      { iom->scheduleLock(fiber, -1); }); // 定时器处理函数就是本协程，也就是超时后会唤醒本协程加入任务队列中继续执行，这里继续点是yield之后，也就是退出sleep继续执行；
                                                          // 这样让cpu跳过sleep时间，sleep时间去执行其他函数，定时器到了模拟sleep时间到了，唤醒本协程继续执行
        fiber->yield();                                   // 等待下次恢复

        // 恢复时，任务队列处理时 下次恢复在这
        return 0;
    }

    int usleep(useconds_t usec)
    {
        if (!corlib::t_hook_enable)
        {
            return usleep_f(usec);
        }

        std::shared_ptr<corlib::Fiber> fiber = corlib::Fiber::GetThis();
        corlib::IOManager *iom = corlib::IOManager::GetThis();
        // 添加定时器以重新调度此协程
        iom->addTimer(usec / 1000, [fiber, iom]()
                      { iom->scheduleLock(fiber); });
        fiber->yield(); // 等待下次恢复
        return 0;
    }

    int nanosleep(const struct timespec *req, struct timespec *rem)
    {
        if (!corlib::t_hook_enable)
        {
            return nanosleep_f(req, rem);
        }

        int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;

        std::shared_ptr<corlib::Fiber> fiber = corlib::Fiber::GetThis();
        corlib::IOManager *iom = corlib::IOManager::GetThis();
        // 添加定时器以重新调度此协程
        iom->addTimer(timeout_ms, [fiber, iom]()
                      { iom->scheduleLock(fiber, -1); });
        fiber->yield(); // 等待下次恢复
        return 0;
    }

    int socket(int domain, int type, int protocol)
    {
        // 检查是否启用了hook功能，如果未启用，直接调用原始的 socket 函数
        if (!corlib::t_hook_enable)
        {
            return socket_f(domain, type, protocol);
        }

        // 调用原始的 socket 函数创建套接字
        int fd = socket_f(domain, type, protocol);
        if (fd == -1)
        {
            // 如果创建套接字失败，打印错误信息并返回错误代码
            std::cerr << "socket() failed: " << strerror(errno) << std::endl;
            return fd;
        }

        // 获取文件描述符上下文，并在管理器中注册该文件描述符
        corlib::FdMgr::GetInstance()->get(fd, true);
        return fd; // 返回文件描述符
    }

    int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms)
    {
        // 检查是否启用了hook功能，如果未启用，直接调用原始的 connect 函数
        if (!corlib::t_hook_enable)
        {
            return connect_f(fd, addr, addrlen);
        }

        // 获取文件描述符上下文
        std::shared_ptr<corlib::FdCtx> ctx = corlib::FdMgr::GetInstance()->get(fd); // 创建/取 文件描述符的Fdctx
        if (!ctx || ctx->isClosed())
        {
            // 如果上下文不存在或文件描述符已关闭，设置错误代码为 EBADF 并返回错误
            errno = EBADF;
            return -1;
        }

        // 如果不是套接字，直接调用原始的 connect 函数
        if (!ctx->isSocket())
        {
            return connect_f(fd, addr, addrlen);
        }

        // 如果套接字已被设置为非阻塞模式，直接调用原始的 connect 函数
        if (ctx->getUserNonblock())
        {
            return connect_f(fd, addr, addrlen);
        }

        // 尝试连接
        int n = connect_f(fd, addr, addrlen);
        if (n == 0)
        {
            // 连接成功，直接返回
            return 0;
        }
        else if (n != -1 && errno != EINPROGRESS)
        {
            // 如果连接失败且错误不是 EINPROGRESS，直接返回错误代码
            return n;
        }

        // 等待写事件准备就绪 -> 连接成功
        corlib::IOManager *iom = corlib::IOManager::GetThis();
        std::shared_ptr<corlib::Timer> timer;
        std::shared_ptr<timer_info> tinfo(new timer_info);
        std::weak_ptr<timer_info> winfo(tinfo);

        // 如果设置了超时时间，添加条件定时器以取消操作
        if (timeout_ms != (uint64_t)-1)
        {
            timer = iom->addConditionTimer(timeout_ms, [winfo, fd, iom]()
                                           {
            auto t = winfo.lock(); // 超时 定时器到了，执行定时器函数，取消事件
            if (!t || t->cancelled) {
                return;
            }
            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, corlib::IOManager::WRITE); }, winfo);
        }

        // 添加写事件到 IOManager，等待事件触发
        int rt = iom->addEvent(fd, corlib::IOManager::WRITE); //其实他之后才等待事件发生或者超时，上面只是注册回调函数
        if (rt == 0)
        {
            // 当前协程让出执行权，等待事件发生
            corlib::Fiber::GetThis()->yield(); // 退出，如果没有任务则进入idle等待事件发生或者超时

            // 恢复执行后，取消定时器（如果存在）
            if (timer)
            {
                timer->cancel();
            }

            // 如果定时器已取消，返回错误
            if (tinfo->cancelled) //只有超时 超时处理函数时才会设置该标志位，然后取消事件时会执行一次事件函数，也就会执行到这里一次
            {
                errno = tinfo->cancelled;
                return -1;
            }
        }
        else
        {
            // 添加事件失败，取消定时器并打印错误信息
            if (timer)
            {
                timer->cancel();
            }
            std::cerr << "connect addEvent(" << fd << ", WRITE) error";
        }

        // 检查连接是否已建立 上面事件成功等到后，这里继续执行
        int error = 0;
        socklen_t len = sizeof(int);
        if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len))
        {
            return -1; // 获取套接字选项失败，返回错误
        }
        if (!error)
        {
            return 0; // 连接成功，返回 0
        }
        else
        {
            errno = error; // 设置错误代码并返回错误
            return -1;
        }
    }

    static uint64_t s_connect_timeout = -1;
    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
    {
        return connect_with_timeout(sockfd, addr, addrlen, s_connect_timeout);
    }

    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
    {
        int fd = do_io(sockfd, accept_f, "accept", corlib::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
        if (fd >= 0)
        {
            corlib::FdMgr::GetInstance()->get(fd, true);
        }
        return fd;
    }

    ssize_t read(int fd, void *buf, size_t count)
    {
        return do_io(fd, read_f, "read", corlib::IOManager::READ, SO_RCVTIMEO, buf, count);
    }

    ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
    {
        return do_io(fd, readv_f, "readv", corlib::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
    }

    ssize_t recv(int sockfd, void *buf, size_t len, int flags)
    {
        return do_io(sockfd, recv_f, "recv", corlib::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
    }

    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
    {
        return do_io(sockfd, recvfrom_f, "recvfrom", corlib::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
    }

    ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
    {
        return do_io(sockfd, recvmsg_f, "recvmsg", corlib::IOManager::READ, SO_RCVTIMEO, msg, flags);
    }

    ssize_t write(int fd, const void *buf, size_t count)
    {
        return do_io(fd, write_f, "write", corlib::IOManager::WRITE, SO_SNDTIMEO, buf, count);
    }

    ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
    {
        return do_io(fd, writev_f, "writev", corlib::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
    }

    ssize_t send(int sockfd, const void *buf, size_t len, int flags)
    {
        return do_io(sockfd, send_f, "send", corlib::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
    }

    ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
    {
        return do_io(sockfd, sendto_f, "sendto", corlib::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags, dest_addr, addrlen);
    }

    ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
    {
        return do_io(sockfd, sendmsg_f, "sendmsg", corlib::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
    }

    int close(int fd)
    {
        if (!corlib::t_hook_enable)
        {
            return close_f(fd);
        }

        std::shared_ptr<corlib::FdCtx> ctx = corlib::FdMgr::GetInstance()->get(fd);

        if (ctx)
        {
            auto iom = corlib::IOManager::GetThis();
            if (iom)
            {
                iom->cancelAll(fd);
            }
            // 删除fdctx
            corlib::FdMgr::GetInstance()->del(fd);
        }
        return close_f(fd);
    }

    int fcntl(int fd, int cmd, ... /* arg */)
    {
        va_list va; // 访问可变参数列表

        va_start(va, cmd);
        switch (cmd)
        {
        case F_SETFL:
        {
            int arg = va_arg(va, int); // 访问下一个int参数
            va_end(va);
            std::shared_ptr<corlib::FdCtx> ctx = corlib::FdMgr::GetInstance()->get(fd);
            if (!ctx || ctx->isClosed() || !ctx->isSocket())
            {
                return fcntl_f(fd, cmd, arg);
            }
            // 用户是否设定了非阻塞
            ctx->setUserNonblock(arg & O_NONBLOCK);
            // 最终是否阻塞由系统设置决定
            if (ctx->getSysNonblock())
            {
                arg |= O_NONBLOCK;
            }
            else
            {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        }
        break;

        case F_GETFL:
        {
            va_end(va);
            int arg = fcntl_f(fd, cmd);
            std::shared_ptr<corlib::FdCtx> ctx = corlib::FdMgr::GetInstance()->get(fd);
            if (!ctx || ctx->isClosed() || !ctx->isSocket())
            {
                return arg;
            }
            // 呈现给用户的是用户设置的值
            if (ctx->getUserNonblock())
            {
                return arg | O_NONBLOCK;
            }
            else
            {
                return arg & ~O_NONBLOCK;
            }
        }
        break;

        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
#ifdef F_SETPIPE_SZ
        case F_SETPIPE_SZ:
#endif
        {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        break;

        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
#ifdef F_GETPIPE_SZ
        case F_GETPIPE_SZ:
#endif
        {
            va_end(va);
            return fcntl_f(fd, cmd);
        }
        break;

        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
        {
            struct flock *arg = va_arg(va, struct flock *);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        break;

        case F_GETOWN_EX:
        case F_SETOWN_EX:
        {
            struct f_owner_exlock *arg = va_arg(va, struct f_owner_exlock *);
            va_end(va);
            return fcntl_f(fd, cmd, arg);
        }
        break;

        default:
            va_end(va);
            return fcntl_f(fd, cmd);
        }
    }

    int ioctl(int fd, unsigned long request, ...)
    {
        va_list va;
        va_start(va, request);
        void *arg = va_arg(va, void *);
        va_end(va);

        if (FIONBIO == request)
        {
            bool user_nonblock = !!*(int *)arg;
            std::shared_ptr<corlib::FdCtx> ctx = corlib::FdMgr::GetInstance()->get(fd);
            if (!ctx || ctx->isClosed() || !ctx->isSocket())
            {
                return ioctl_f(fd, request, arg);
            }
            ctx->setUserNonblock(user_nonblock);
        }
        return ioctl_f(fd, request, arg);
    }

    int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
    {
        return getsockopt_f(sockfd, level, optname, optval, optlen);
    }

    int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
    {
        if (!corlib::t_hook_enable)
        {
            return setsockopt_f(sockfd, level, optname, optval, optlen);
        }

        if (level == SOL_SOCKET)
        {
            if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)
            {
                std::shared_ptr<corlib::FdCtx> ctx = corlib::FdMgr::GetInstance()->get(sockfd);
                if (ctx)
                {
                    const timeval *v = (const timeval *)optval;
                    ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
                }
            }
        }
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }

} // extern "C"
