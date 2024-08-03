#include "timer.h"

namespace corlib
{

    // 取消定时器
    bool Timer::cancel()
    {
        std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);

        // 如果定时器已经没有回调函数，直接返回 false
        if (m_cb == nullptr)
        {
            return false;
        }
        else
        {
            m_cb = nullptr; // 将回调函数置为空
        }

        // 从定时器管理器中删除该定时器
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it != m_manager->m_timers.end())
        {
            m_manager->m_timers.erase(it);
        }
        return true;
    }

    // 刷新定时器，只会向后调整
    bool Timer::refresh()
    {
        std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);

        if (!m_cb)
        {
            return false;
        }

        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end())
        {
            return false;
        }

        m_manager->m_timers.erase(it);                                               // 删除旧的定时器
        m_next = std::chrono::system_clock::now() + std::chrono::milliseconds(m_ms); // 重新计算超时时间
        m_manager->m_timers.insert(shared_from_this());                              // 插入新的定时器
        return true;
    }

    // 重置定时器
    bool Timer::reset(uint64_t ms, bool from_now)
    {
        if (ms == m_ms && !from_now)
        {
            return true;
        }

        {
            std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);

            if (!m_cb)
            {
                return false;
            }

            auto it = m_manager->m_timers.find(shared_from_this());
            if (it == m_manager->m_timers.end())
            {
                return false;
            }
            m_manager->m_timers.erase(it); // 删除旧的定时器
        }

        // 重新计算超时时间
        auto start = from_now ? std::chrono::system_clock::now() : m_next - std::chrono::milliseconds(m_ms);
        m_ms = ms;
        m_next = start + std::chrono::milliseconds(m_ms);
        m_manager->addTimer(shared_from_this()); // 插入新的定时器
        return true;
    }

    // 定时器构造函数
    Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager) : m_recurring(recurring), m_ms(ms), m_cb(cb), m_manager(manager)
    {
        auto now = std::chrono::system_clock::now();
        m_next = now + std::chrono::milliseconds(m_ms); // 计算超时时间
    }

    // 比较器，用于在定时器集合中排序
    bool Timer::Comparator::operator()(const std::shared_ptr<Timer> &lhs, const std::shared_ptr<Timer> &rhs) const
    {
        assert(lhs != nullptr && rhs != nullptr);
        return lhs->m_next < rhs->m_next;
    }

    // 定时器管理器构造函数
    TimerManager::TimerManager()
    {
        m_previouseTime = std::chrono::system_clock::now(); // 记录当前时间
    }

    // 定时器管理器析构函数
    TimerManager::~TimerManager()
    {
    }

    // 添加定时器
    std::shared_ptr<Timer> TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring)
    {
        std::shared_ptr<Timer> timer(new Timer(ms, cb, recurring, this));
        addTimer(timer);
        return timer;
    }

    // 如果条件存在，执行回调函数
    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb)
    {
        std::shared_ptr<void> tmp = weak_cond.lock();
        if (tmp)
        {
            cb();
        }
    }

    // 添加条件定时器
    std::shared_ptr<Timer> TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring)
    {
        return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
    }

    // 获取下一个定时器的超时时间
    uint64_t TimerManager::getNextTimer()
    {
        std::shared_lock<std::shared_mutex> read_lock(m_mutex);

        // 重置 m_tickled
        m_tickled = false;

        if (m_timers.empty())
        {
            // 返回最大值
            return ~0ull;
        }

        auto now = std::chrono::system_clock::now();
        auto time = (*m_timers.begin())->m_next;

        if (now >= time)
        {
            // 已经有定时器超时
            return 0;
        }
        else
        {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time - now);
            return static_cast<uint64_t>(duration.count());
        }
    }

    // 列出所有已过期的定时器回调函数
    void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs)
    {
        auto now = std::chrono::system_clock::now();

        std::unique_lock<std::shared_mutex> write_lock(m_mutex);

        bool rollover = detectClockRollover();

        // 检测回退或超时，清理所有超时的定时器
        while (!m_timers.empty() && (rollover || (*m_timers.begin())->m_next <= now))
        {
            std::shared_ptr<Timer> temp = *m_timers.begin();
            m_timers.erase(m_timers.begin());

            cbs.push_back(temp->m_cb);

            if (temp->m_recurring)
            {
                // 重新加入时间堆
                temp->m_next = now + std::chrono::milliseconds(temp->m_ms);
                m_timers.insert(temp);
            }
            else
            {
                // 清理回调函数
                temp->m_cb = nullptr;
            }
        }
    }

    // 判断是否有定时器
    bool TimerManager::hasTimer()
    {
        std::shared_lock<std::shared_mutex> read_lock(m_mutex);
        return !m_timers.empty();
    }

    // 添加定时器并唤醒调度器
    void TimerManager::addTimer(std::shared_ptr<Timer> timer)
    {
        bool at_front = false;
        {
            std::unique_lock<std::shared_mutex> write_lock(m_mutex);
            auto it = m_timers.insert(timer).first;
            at_front = (it == m_timers.begin()) && !m_tickled;

            // 只唤醒一次直到有线程执行 getNextTime()
            if (at_front)
            {
                m_tickled = true;
            }
        }

        if (at_front)
        {
            // 唤醒调度器
            onTimerInsertedAtFront();
        }
    }

    // 检测时钟回滚
    bool TimerManager::detectClockRollover()
    {
        bool rollover = false;
        auto now = std::chrono::system_clock::now();
        if (now < (m_previouseTime - std::chrono::milliseconds(60 * 60 * 1000)))
        {
            rollover = true;
        }
        m_previouseTime = now;
        return rollover;
    }

}

