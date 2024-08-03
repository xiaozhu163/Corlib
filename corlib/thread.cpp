#include "thread.h"

#include <sys/syscall.h> // 包含 syscall 函数的头文件
#include <iostream>      // 标准输入输出流
#include <unistd.h>      // POSIX 操作系统 API 的头文件

namespace corlib
{

    // 线程信息
    static thread_local Thread *t_thread = nullptr;            // 当前线程对象
    static thread_local std::string t_thread_name = "UNKNOWN"; // 当前线程名称

    // 获取当前线程的线程ID
    pid_t Thread::GetThreadId()
    {
        return syscall(SYS_gettid);
    }

    // 获取当前线程对象
    Thread *Thread::GetThis()
    {
        return t_thread;
    }

    // 获取当前线程的名称
    const std::string &Thread::GetName()
    {
        return t_thread_name;
    }

    // 设置当前线程的名称
    void Thread::SetName(const std::string &name)
    {
        if (t_thread)
        {
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }

    // 线程类构造函数
    Thread::Thread(std::function<void()> cb, const std::string &name) : m_cb(cb), m_name(name)
    {
        int rt = pthread_create(&m_thread, nullptr, &Thread::run, this); // 创建线程
        if (rt)
        {
            std::cerr << "pthread_create thread fail, rt=" << rt << " name=" << name;
            throw std::logic_error("pthread_create error"); // 线程创建失败，抛出异常
        }
        // 等待线程函数完成初始化
        m_semaphore.wait();
    }

    // 线程类析构函数
    Thread::~Thread()
    {
        if (m_thread)
        {
            pthread_detach(m_thread); // 分离线程
            m_thread = 0;
        }
    }

    // 等待线程结束
    void Thread::join()
    {
        if (m_thread)
        {
            int rt = pthread_join(m_thread, nullptr); // 等待线程结束
            if (rt)
            {
                std::cerr << "pthread_join failed, rt = " << rt << ", name = " << m_name << std::endl;
                throw std::logic_error("pthread_join error"); // 等待失败，抛出异常
            }
            m_thread = 0;
        }
    }

    // 线程的主函数
    void *Thread::run(void *arg)
    {
        Thread *thread = (Thread *)arg; // 获取线程对象

        // 初始化线程局部变量
        t_thread = thread;
        t_thread_name = thread->m_name;
        thread->m_id = GetThreadId();                                             // 获取线程ID
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str()); // 设置线程名称

        std::function<void()> cb;
        cb.swap(thread->m_cb); // swap -> 可以减少m_cb中智能指针的引用计数

        // 初始化完成，通知主线程
        thread->m_semaphore.signal();

        // 执行传入的回调函数
        cb();
        return 0;
    }

} // namespace corlib
