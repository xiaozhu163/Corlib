#include <unistd.h>    // for pipe, close, read, write
#include <sys/epoll.h> // for epoll_create, epoll_ctl, epoll_wait
#include <fcntl.h>     // for fcntl
#include <cstring>     // for strerror

#include "ioscheduler.h" // Custom header file for IOManager and related classes

static bool debug = false; // Debug flag

namespace corlib
{

    // 获取当前线程的IOManager实例
    IOManager *IOManager::GetThis()
    {
        return dynamic_cast<IOManager *>(Scheduler::GetThis());  // 这里获取到的 就是 static thread_local Scheduler *t_scheduler = nullptr; // 当前线程上的调度器指针，然后转换成IOManager，本质还是属于线程局部静态变量
    }

    // 获取指定事件的EventContext
    IOManager::FdContext::EventContext &IOManager::FdContext::getEventContext(Event event)
    {
        assert(event == READ || event == WRITE);
        switch (event)
        {
        case READ:
            return read;
        case WRITE:
            return write;
        }
        throw std::invalid_argument("Unsupported event type");
    }

    // 重置EventContext
    void IOManager::FdContext::resetEventContext(EventContext &ctx)
    {
        ctx.scheduler = nullptr;
        ctx.fiber.reset();
        ctx.cb = nullptr;
    }

    // 触发事件，不加锁
    void IOManager::FdContext::triggerEvent(IOManager::Event event)
    {
        assert(events & event);

        // 删除事件
        events = (Event)(events & ~event);

        // 触发事件
        EventContext &ctx = getEventContext(event);
        if (ctx.cb)
        {
            // 调用scheduleLock(std::function<void()>* f, int thr)
            ctx.scheduler->scheduleLock(&ctx.cb);
        }
        else
        {
            // 调用scheduleLock(std::shared_ptr<Fiber>* f, int thr)
            ctx.scheduler->scheduleLock(&ctx.fiber);
        }

        // 重置事件上下文
        resetEventContext(ctx);
        return;
    }

    // IOManager构造函数，初始化epoll和管道
    IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
        : Scheduler(threads, use_caller, name), TimerManager()
    {
        // 创建epoll文件描述符
        m_epfd = epoll_create(5000);
        assert(m_epfd > 0);

        // 创建管道
        int rt = pipe(m_tickleFds);
        assert(!rt);

        // 添加读事件到epoll
        epoll_event event;
        event.events = EPOLLIN | EPOLLET; // 边缘触发
        event.data.fd = m_tickleFds[0];

        // 设置非阻塞
        rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
        assert(!rt);

        // 将读事件添加到epoll
        rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
        assert(!rt);

        // 初始化上下文大小
        contextResize(32);

        // 启动调度器
        start();
    }

    // IOManager析构函数，释放资源
    IOManager::~IOManager()
    {
        stop();
        close(m_epfd);
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);

        for (size_t i = 0; i < m_fdContexts.size(); ++i)
        {
            if (m_fdContexts[i])
            {
                delete m_fdContexts[i];
            }
        }
    }

    // 调整上下文大小，不加锁
    void IOManager::contextResize(size_t size)
    {
        m_fdContexts.resize(size); // 这里他妈都准备好了，遇到文件描述符直接在这用就行，不用再单独创建fd_context了

        for (size_t i = 0; i < m_fdContexts.size(); ++i)
        {
            if (m_fdContexts[i] == nullptr)
            {
                m_fdContexts[i] = new FdContext();
                m_fdContexts[i]->fd = i;
            }
        }
    }

    // 添加事件
    int IOManager::addEvent(int fd, Event event, std::function<void()> cb)
    {
        // 尝试找到FdContext
        FdContext *fd_ctx = nullptr;

        std::shared_lock<std::shared_mutex> read_lock(m_mutex);
        if ((int)m_fdContexts.size() > fd)
        {
            fd_ctx = m_fdContexts[fd];
            read_lock.unlock();
        }
        else
        {
            read_lock.unlock();
            std::unique_lock<std::shared_mutex> write_lock(m_mutex);
            contextResize(fd * 1.5);
            fd_ctx = m_fdContexts[fd];
        }

        std::lock_guard<std::mutex> lock(fd_ctx->mutex);

        // 事件已添加
        if (fd_ctx->events & event)
        {
            return -1;
        }

        // 添加新事件
        int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        epoll_event epevent;
        epevent.events = EPOLLET | fd_ctx->events | event;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if (rt)
        {
            std::cerr << "addEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
            return -1;
        }

        ++m_pendingEventCount;

        // 更新FdContext
        fd_ctx->events = (Event)(fd_ctx->events | event);

        // 更新事件上下文
        FdContext::EventContext &event_ctx = fd_ctx->getEventContext(event);
        assert(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);
        event_ctx.scheduler = Scheduler::GetThis();
        if (cb)
        {
            event_ctx.cb.swap(cb);
        }
        else
        {
            event_ctx.fiber = Fiber::GetThis(); // 如果没有回调函数，那么就是回调函数就是当前协程
            assert(event_ctx.fiber->getState() == Fiber::RUNNING);
        }
        return 0;
    }

    // 删除事件
    bool IOManager::delEvent(int fd, Event event)
    {
        // 尝试找到FdContext
        FdContext *fd_ctx = nullptr;

        std::shared_lock<std::shared_mutex> read_lock(m_mutex);
        if ((int)m_fdContexts.size() > fd)
        {
            fd_ctx = m_fdContexts[fd];
            read_lock.unlock();
        }
        else
        {
            read_lock.unlock();
            return false;
        }

        std::lock_guard<std::mutex> lock(fd_ctx->mutex);

        // 事件不存在
        if (!(fd_ctx->events & event))
        {
            return false;
        }

        // 删除事件
        Event new_events = (Event)(fd_ctx->events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if (rt)
        {
            std::cerr << "delEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
            return -1;
        }

        --m_pendingEventCount;

        // 更新FdContext
        fd_ctx->events = new_events;

        // 重置事件上下文
        FdContext::EventContext &event_ctx = fd_ctx->getEventContext(event);
        fd_ctx->resetEventContext(event_ctx);
        return true;
    }

    // 取消事件
    bool IOManager::cancelEvent(int fd, Event event)
    {
        // 尝试找到FdContext
        FdContext *fd_ctx = nullptr;

        std::shared_lock<std::shared_mutex> read_lock(m_mutex);
        if ((int)m_fdContexts.size() > fd)
        {
            fd_ctx = m_fdContexts[fd];
            read_lock.unlock();
        }
        else
        {
            read_lock.unlock();
            return false;
        }

        std::lock_guard<std::mutex> lock(fd_ctx->mutex);

        // 事件不存在
        if (!(fd_ctx->events & event))
        {
            return false;
        }

        // 删除事件
        Event new_events = (Event)(fd_ctx->events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if (rt)
        {
            std::cerr << "cancelEvent::epoll_ctl failed: " << strerror(errno) << std::endl;
            return -1;
        }

        --m_pendingEventCount;

        // 触发事件并更新FdContext
        fd_ctx->triggerEvent(event);
        return true;
    }

    // 取消所有事件
    bool IOManager::cancelAll(int fd)
    {
        // 尝试找到FdContext
        FdContext *fd_ctx = nullptr;

        std::shared_lock<std::shared_mutex> read_lock(m_mutex);
        if ((int)m_fdContexts.size() > fd)
        {
            fd_ctx = m_fdContexts[fd];
            read_lock.unlock();
        }
        else
        {
            read_lock.unlock();
            return false;
        }

        std::lock_guard<std::mutex> lock(fd_ctx->mutex);

        // 没有事件存在
        if (!fd_ctx->events)
        {
            return false;
        }

        // 删除所有事件
        int op = EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = 0;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if (rt)
        {
            std::cerr << "IOManager::epoll_ctl failed: " << strerror(errno) << std::endl;
            return -1;
        }

        // 触发事件并更新FdContext
        if (fd_ctx->events & READ)
        {
            fd_ctx->triggerEvent(READ);
            --m_pendingEventCount;
        }

        if (fd_ctx->events & WRITE)
        {
            fd_ctx->triggerEvent(WRITE);
            --m_pendingEventCount;
        }

        assert(fd_ctx->events == 0);
        return true;
    }

    // 通知线程
    void IOManager::tickle()
    {
        // 没有空闲线程
        if (!hasIdleThreads())
        {
            return;
        }
        int rt = write(m_tickleFds[1], "T", 1);
        assert(rt == 1);
    }

    // 检查是否停止
    bool IOManager::stopping()
    {
        uint64_t timeout = getNextTimer();
        // 没有剩余的定时器且没有待处理事件并且Scheduler停止
        return timeout == ~0ull && m_pendingEventCount == 0 && Scheduler::stopping();
    }

    // 空闲状态
    void IOManager::idle()
    {
        static const uint64_t MAX_EVENTS = 256;
        std::unique_ptr<epoll_event[]> events(new epoll_event[MAX_EVENTS]);

        while (true)
        {
            if (debug)
                std::cout << "IOManager::idle(), run in thread: " << Thread::GetThreadId() << std::endl;

            if (stopping())
            {
                if (debug)
                    std::cout << "name = " << getName() << " idle exits in thread: " << Thread::GetThreadId() << std::endl;
                break;
            }

            // 阻塞在epoll_wait
            int rt = 0;
            while (true)
            {
                static const uint64_t MAX_TIMEOUT = 5000;
                uint64_t next_timeout = getNextTimer();
                next_timeout = std::min(next_timeout, MAX_TIMEOUT);

                rt = epoll_wait(m_epfd, events.get(), MAX_EVENTS, (int)next_timeout);
                // EINTR -> 重试
                if (rt < 0 && errno == EINTR)
                {
                    continue;
                }
                else
                {
                    break;
                }
            };
W
            // 收集所有过期的定时器
            std::vector<std::functiWon<void()>> cbs;
            listExpiredCb(cbs);
            if (!cbs.empty())
            {
                for (const auto &cb : cbs)
                {
                    scheduleLock(cb);
                }
                cbs.clear();
            }

            // 收集所有准备好的事件
            for (int i = 0; i < rt; ++i)
            {
                epoll_event &event = events[i];

                // 处理tickle事件
                if (event.data.fd == m_tickleFds[0])
                {
                    uint8_t dummy[256];
                    // 边缘触发 -> 耗尽
                    while (read(m_tickleFds[0], dummy, sizeof(dummy)) > 0)
                        ;
                    continue;
                }

                // 处理其他事件
                FdContext *fd_ctx = (FdContext *)event.data.ptr;
                std::lock_guard<std::mutex> lock(fd_ctx->mutex);

                // 将EPOLLERR或EPOLLHUP转换为读或写事件
                if (event.events & (EPOLLERR | EPOLLHUP))
                {
                    event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
                }
                // 当前轮次epoll_wait期间发生的事件
                int real_events = NONE;
                if (event.events & EPOLLIN)
                {
                    real_events |= READ;
                }
                if (event.events & EPOLLOUT)
                {
                    real_events |= WRITE;
                }

                if ((fd_ctx->events & real_events) == NONE)
                {
                    continue;
                }

                // 删除已经发生的事件
                int left_events = (fd_ctx->events & ~real_events);
                int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
                event.events = EPOLLET | left_events;

                int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
                if (rt2)
                {
                    std::cerr << "idle::epoll_ctl failed: " << strerror(errno) << std::endl;
                    continue;
                }

                // 调度回调并更新FdContext和事件上下文
                if (real_events & READ)
                {
                    fd_ctx->triggerEvent(READ);
                    --m_pendingEventCount;
                }
                if (real_events & WRITE)
                {
                    fd_ctx->triggerEvent(WRITE);
                    --m_pendingEventCount;
                }
            } // 结束 for

            Fiber::GetThis()->yield();

        } // 结束 while(true)
    }

    // 当定时器插入到队列前端时调用
    void IOManager::onTimerInsertedAtFront()
    {
        tickle();
    }

} // end namespace corlib
