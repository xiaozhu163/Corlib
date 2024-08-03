#include "scheduler.h"

static bool debug = false; // 是否启用调试

namespace corlib
{

	static thread_local Scheduler *t_scheduler = nullptr; // 当前线程上的调度器指针

	Scheduler *Scheduler::GetThis()
	{
		return t_scheduler;
	}

	void Scheduler::SetThis()
	{
		t_scheduler = this;
	}

	// 调度器构造函数
	Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name) : m_useCaller(use_caller), m_name(name)
	{
		assert(threads > 0 && Scheduler::GetThis() == nullptr); // 保证线程数大于0且当前线程没有调度器

		SetThis(); // 设置当前调度器为该实例

		Thread::SetName(m_name); // 设置线程名称

		// 使用主线程当作工作线程
		if (use_caller)
		{
			threads--; // 如果使用主线程，则需要减少一个线程数

			// 创建主协程
			Fiber::GetThis();

			// 创建调度协程
			m_schedulerFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false)); // false -> 该调度协程退出后将返回主协程
			Fiber::SetSchedulerFiber(m_schedulerFiber.get());

			m_rootThread = Thread::GetThreadId(); // 获取主线程ID
			m_threadIds.push_back(m_rootThread);  // 将主线程ID加入线程ID列表
		}

		m_threadCount = threads; // 设置工作线程数
		if (debug)
			std::cout << "Scheduler::Scheduler() success\n";
	}

	// 调度器析构函数
	Scheduler::~Scheduler()
	{
		assert(stopping() == true); // 确保调度器已经停止
		if (GetThis() == this)
		{
			t_scheduler = nullptr;
		}
		if (debug)
			std::cout << "Scheduler::~Scheduler() success\n";
	}

	// 启动调度器
	void Scheduler::start()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_stopping)
		{
			std::cerr << "Scheduler is stopped" << std::endl;
			return;
		}

		assert(m_threads.empty());
		m_threads.resize(m_threadCount);
		for (size_t i = 0; i < m_threadCount; i++)
		{
			m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
			m_threadIds.push_back(m_threads[i]->getId());
		}
		if (debug)
			std::cout << "Scheduler::start() success\n";
	}

	// 调度器的运行函数
	void Scheduler::run()
	{
		int thread_id = Thread::GetThreadId();
		if (debug)
			std::cout << "Schedule::run() starts in thread: " << thread_id << std::endl;

		set_hook_enable(true); // 启用hook

		SetThis(); // 设置当前调度器

		// 运行在新创建的线程 -> 需要创建主协程
		if (thread_id != m_rootThread)
		{
			Fiber::GetThis();
		}

		std::shared_ptr<Fiber> idle_fiber = std::make_shared<Fiber>(std::bind(&Scheduler::idle, this));
		ScheduleTask task;

		while (true)
		{
			task.reset();
			bool tickle_me = false;

			{
				std::lock_guard<std::mutex> lock(m_mutex);
				auto it = m_tasks.begin();
				// 遍历任务队列
				while (it != m_tasks.end())
				{
					if (it->thread != -1 && it->thread != thread_id)
					{
						it++;
						tickle_me = true;
						continue;
					}

					// 取出任务
					assert(it->fiber || it->cb);
					task = *it;
					m_tasks.erase(it);
					m_activeThreadCount++;
					break;
				}
				tickle_me = tickle_me || (it != m_tasks.end());
			}

			if (tickle_me)
			{
				tickle();
			}

			// 执行任务
			if (task.fiber)
			{
				{
					std::lock_guard<std::mutex> lock(task.fiber->m_mutex);
					if (task.fiber->getState() != Fiber::TERM)
					{
						task.fiber->resume();
					}
				}
				m_activeThreadCount--;
				task.reset();
			}
			else if (task.cb)
			{
				std::shared_ptr<Fiber> cb_fiber = std::make_shared<Fiber>(task.cb);
				{
					std::lock_guard<std::mutex> lock(cb_fiber->m_mutex);
					cb_fiber->resume();
				}
				m_activeThreadCount--;
				task.reset();
			}
			// 无任务 -> 执行空闲协程
			else
			{
				// 系统关闭 -> idle协程将从死循环跳出并结束 -> 此时的idle协程状态为TERM -> 再次进入将跳出循环并退出run()
				if (idle_fiber->getState() == Fiber::TERM)
				{
					if (debug)
						std::cout << "Schedule::run() ends in thread: " << thread_id << std::endl;
					break;
				}
				m_idleThreadCount++;
				idle_fiber->resume();
				m_idleThreadCount--;
			}
		}
	}

	// 停止调度器
	void Scheduler::stop()
	{
		if (debug)
			std::cout << "Schedule::stop() starts in thread: " << Thread::GetThreadId() << std::endl;

		if (stopping())
		{
			return;
		}

		m_stopping = true;

		if (m_useCaller)
		{
			assert(GetThis() == this);
		}
		else
		{
			assert(GetThis() != this);
		}

		for (size_t i = 0; i < m_threadCount; i++)
		{
			tickle();
		}

		if (m_schedulerFiber)
		{
			tickle();
		}

		if (m_schedulerFiber)
		{
			m_schedulerFiber->resume();
			if (debug)
				std::cout << "m_schedulerFiber ends in thread:" << Thread::GetThreadId() << std::endl;
		}

		std::vector<std::shared_ptr<Thread>> thrs;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			thrs.swap(m_threads);
		}

		for (auto &i : thrs)
		{
			i->join();
		}
		if (debug)
			std::cout << "Schedule::stop() ends in thread:" << Thread::GetThreadId() << std::endl;
	}

	// 唤醒调度器
	void Scheduler::tickle()
	{
	}

	// 空闲处理函数
	void Scheduler::idle()
	{
		while (!stopping())
		{
			if (debug)
				std::cout << "Scheduler::idle(), sleeping in thread: " << Thread::GetThreadId() << std::endl;
			sleep(1);
			Fiber::GetThis()->yield();
		}
	}

	// 判断调度器是否可以停止
	bool Scheduler::stopping()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_stopping && m_tasks.empty() && m_activeThreadCount == 0;
	}

}
