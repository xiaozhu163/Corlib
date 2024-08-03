#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include <list>

#include "noncopyable.h"
#include "fiber.h"

namespace corlib {

/**
 *  信号量
 */
class Semaphore : Noncopyable {
public:
    /**
     *  构造函数
     * @param[in] count 信号量值的大小
     */
    Semaphore(uint32_t count = 0);

    /**
     *  析构函数
     */
    ~Semaphore();

    /**
     *  获取信号量
     */
    void wait();

    /**
     *  释放信号量
     */
    void notify();
private:
    sem_t m_semaphore;
};

/**
 *  局部锁的模板实现
 */
template<class T>
struct ScopedLockImpl {
public:
    /**
     *  构造函数
     * @param[in] mutex Mutex
     */
    ScopedLockImpl(T& mutex)
        :m_mutex(mutex) {
        m_mutex.lock();
        m_locked = true;
    }

    /**
     *  析构函数,自动释放锁
     */
    ~ScopedLockImpl() {
        unlock();
    }

    /**
     *  加锁
     */
    void lock() {
        if(!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }

    /**
     *  解锁
     */
    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    /// mutex
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};

/**
 *  局部读锁模板实现
 */
template<class T>
struct ReadScopedLockImpl {
public:
    /**
     *  构造函数
     * @param[in] mutex 读写锁
     */
    ReadScopedLockImpl(T& mutex)
        :m_mutex(mutex) {
        m_mutex.rdlock();
        m_locked = true;
    }

    /**
     *  析构函数,自动释放锁
     */
    ~ReadScopedLockImpl() {
        unlock();
    }

    /**
     *  上读锁
     */
    void lock() {
        if(!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    /**
     *  释放锁
     */
    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    /// mutex
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};

/**
 *  局部写锁模板实现
 */
template<class T>
struct WriteScopedLockImpl {
public:
    /**
     *  构造函数
     * @param[in] mutex 读写锁
     */
    WriteScopedLockImpl(T& mutex)
        :m_mutex(mutex) {
        m_mutex.wrlock();
        m_locked = true;
    }

    /**
     *  析构函数
     */
    ~WriteScopedLockImpl() {
        unlock();
    }

    /**
     *  上写锁
     */
    void lock() {
        if(!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    /**
     *  解锁
     */
    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    /// Mutex
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};

/**
 *  互斥量
 */
class Mutex : Noncopyable {
public: 
    /// 局部锁
    typedef ScopedLockImpl<Mutex> Lock;

    /**
     *  构造函数
     */
    Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    /**
     *  析构函数
     */
    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    /**
     *  加锁
     */
    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    /**
     *  解锁
     */
    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }
private:
    /// mutex
    pthread_mutex_t m_mutex;
};

/**
 *  空锁(用于调试)
 */
class NullMutex : Noncopyable{
public:
    /// 局部锁
    typedef ScopedLockImpl<NullMutex> Lock;

    /**
     *  构造函数
     */
    NullMutex() {}

    /**
     *  析构函数
     */
    ~NullMutex() {}

    /**
     *  加锁
     */
    void lock() {}

    /**
     *  解锁
     */
    void unlock() {}
};

/**
 *  读写互斥量
 */
class RWMutex : Noncopyable{
public:

    /// 局部读锁
    typedef ReadScopedLockImpl<RWMutex> ReadLock;

    /// 局部写锁
    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    /**
     *  构造函数
     */
    RWMutex() {
        pthread_rwlock_init(&m_lock, nullptr);
    }
    
    /**
     *  析构函数
     */
    ~RWMutex() {
        pthread_rwlock_destroy(&m_lock);
    }

    /**
     *  上读锁
     */
    void rdlock() {
        pthread_rwlock_rdlock(&m_lock);
    }

    /**
     *  上写锁
     */
    void wrlock() {
        pthread_rwlock_wrlock(&m_lock);
    }

    /**
     *  解锁
     */
    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }
private:
    /// 读写锁
    pthread_rwlock_t m_lock;
};

/**
 *  空读写锁(用于调试)
 */
class NullRWMutex : Noncopyable {
public:
    /// 局部读锁
    typedef ReadScopedLockImpl<NullMutex> ReadLock;
    /// 局部写锁
    typedef WriteScopedLockImpl<NullMutex> WriteLock;

    /**
     *  构造函数
     */
    NullRWMutex() {}
    /**
     *  析构函数
     */
    ~NullRWMutex() {}

    /**
     *  上读锁
     */
    void rdlock() {}

    /**
     *  上写锁
     */
    void wrlock() {}
    /**
     *  解锁
     */
    void unlock() {}
};

/**
 *  自旋锁
 */
class Spinlock : Noncopyable {
public:
    /// 局部锁
    typedef ScopedLockImpl<Spinlock> Lock;

    /**
     *  构造函数
     */
    Spinlock() {
        pthread_spin_init(&m_mutex, 0);
    }

    /**
     *  析构函数
     */
    ~Spinlock() {
        pthread_spin_destroy(&m_mutex);
    }

    /**
     *  上锁
     */
    void lock() {
        pthread_spin_lock(&m_mutex);
    }

    /**
     *  解锁
     */
    void unlock() {
        pthread_spin_unlock(&m_mutex);
    }
private:
    /// 自旋锁
    pthread_spinlock_t m_mutex;
};

/**
 *  原子锁
 */
class CASLock : Noncopyable {
public:
    /// 局部锁
    typedef ScopedLockImpl<CASLock> Lock;

    /**
     *  构造函数
     */
    CASLock() {
        m_mutex.clear();
    }

    /**
     *  析构函数
     */
    ~CASLock() {
    }

    /**
     *  上锁
     */
    void lock() {
        while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    }

    /**
     *  解锁
     */
    void unlock() {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }
private:
    /// 原子状态
    volatile std::atomic_flag m_mutex;
};

class Scheduler;
class FiberSemaphore : Noncopyable {
public:
    typedef Spinlock MutexType;

    FiberSemaphore(size_t initial_concurrency = 0);
    ~FiberSemaphore();

    bool tryWait();
    void wait();
    void notify();

    size_t getConcurrency() const { return m_concurrency;}
    void reset() { m_concurrency = 0;}
private:
    MutexType m_mutex;
    std::list<std::pair<Scheduler*, Fiber::ptr> > m_waiters;
    size_t m_concurrency;
};



}

#endif
