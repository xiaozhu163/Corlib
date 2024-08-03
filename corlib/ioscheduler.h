#ifndef __IOMANAGER_H__
#define __IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace corlib
{

    // 工作流程
    // 1. 注册一个事件 -> 2. 等待其准备就绪 -> 3. 调度回调 -> 4. 注销事件 -> 5. 运行回调
    class IOManager : public Scheduler, public TimerManager
    {
    public:
        // 定义事件类型
        enum Event
        {
            NONE = 0x0, // 无事件
            READ = 0x1, // 读事件，对应 EPOLLIN
            WRITE = 0x4 // 写事件，对应 EPOLLOUT
        };

    private:
        // 文件描述符上下文
        struct FdContext
        {
            // 事件上下文
            struct EventContext
            {
                // 调度器
                Scheduler *scheduler = nullptr;
                // 回调协程
                std::shared_ptr<Fiber> fiber;
                // 回调函数
                std::function<void()> cb;
            };

            // 读事件上下文
            EventContext read;
            // 写事件上下文
            EventContext write;
            // 文件描述符
            int fd = 0;
            // 已注册的事件
            Event events = NONE;
            // 互斥锁
            std::mutex mutex;

            // 获取事件上下文
            EventContext &getEventContext(Event event);
            // 重置事件上下文
            void resetEventContext(EventContext &ctx);
            // 触发事件
            void triggerEvent(Event event);
        };

    public:
        // 构造函数
        IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");
        // 析构函数
        ~IOManager();

        // 添加事件
        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
        // 删除事件
        bool delEvent(int fd, Event event);
        // 取消事件并触发其回调
        bool cancelEvent(int fd, Event event);
        // 取消所有事件并触发其回调
        bool cancelAll(int fd);

        // 获取当前 IOManager 实例，在任何时候都可以调用，返回当前线程的 IOManager 实例
        static IOManager *GetThis();

    protected:
        // 唤醒调度器
        void tickle() override;

        // 判断是否可以停止
        bool stopping() override;

        // 空闲处理函数
        void idle() override;

        // 当定时器插入到队列头部时调用
        void onTimerInsertedAtFront() override;

        // 调整上下文大小
        void contextResize(size_t size);

    private:
        // epoll 文件描述符
        int m_epfd = 0;
        // 管道文件描述符，fd[0] 读，fd[1] 写
        int m_tickleFds[2];
        // 挂起事件计数
        std::atomic<size_t> m_pendingEventCount = {0};
        // 共享互斥锁
        std::shared_mutex m_mutex;
        // 存储每个文件描述符的上下文
        std::vector<FdContext *> m_fdContexts;
    };

} // end namespace corlib

#endif
