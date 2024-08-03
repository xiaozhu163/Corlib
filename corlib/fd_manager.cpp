#include "fd_manager.h"
#include "hook.h"

#include <sys/types.h> // 包含系统类型头文件
#include <sys/stat.h>  // 包含系统状态头文件
#include <unistd.h>	   // 包含UNIX标准头文件

namespace corlib
{

	// 实例化模板类Singleton<FdManager>
	template class Singleton<FdManager>;

	// 静态变量需要在类外定义
	template <typename T>
	T *Singleton<T>::instance = nullptr; // 初始化静态实例指针为nullptr

	template <typename T>
	std::mutex Singleton<T>::mutex; // 初始化静态互斥锁

	// FdCtx构造函数
	FdCtx::FdCtx(int fd) : m_fd(fd) // 初始化成员变量m_fd
	{
		init(); // 调用init函数进行初始化
	}

	// FdCtx析构函数
	FdCtx::~FdCtx()
	{
	}

	// 初始化函数
	bool FdCtx::init()
	{
		// 如果已经初始化，直接返回true
		if (m_isInit)
		{
			return true;
		}

		struct stat statbuf;
		// 获取文件描述符的状态信息
		if (-1 == fstat(m_fd, &statbuf))
		{
			// 如果获取失败，设置初始化标志和socket标志为false
			m_isInit = false;
			m_isSocket = false;
		}
		else
		{
			// 如果获取成功，设置初始化标志为true，检查是否为socket
			m_isInit = true;
			m_isSocket = S_ISSOCK(statbuf.st_mode);
		}

		// 如果是socket，则设置为非阻塞模式
		if (m_isSocket)
		{
			// 调用原始fcntl函数获取文件描述符的状态标志
			int flags = fcntl_f(m_fd, F_GETFL, 0);
			if (!(flags & O_NONBLOCK))
			{
				// 如果不是非阻塞模式，则设置为非阻塞模式
				fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
			}
			m_sysNonblock = true; // 设置系统非阻塞标志为true
		}
		else
		{
			m_sysNonblock = false; // 如果不是socket，设置系统非阻塞标志为false
		}

		return m_isInit; // 返回初始化标志
	}

	// 设置超时时间
	void FdCtx::setTimeout(int type, uint64_t v)
	{
		// 根据类型设置接收或发送超时时间
		if (type == SO_RCVTIMEO)
		{
			m_recvTimeout = v;
		}
		else
		{
			m_sendTimeout = v;
		}
	}

	// 获取超时时间
	uint64_t FdCtx::getTimeout(int type)
	{
		// 根据类型获取接收或发送超时时间
		if (type == SO_RCVTIMEO)
		{
			return m_recvTimeout;
		}
		else
		{
			return m_sendTimeout;
		}
	}

	// FdManager构造函数
	FdManager::FdManager()
	{
		// 初始化存储文件描述符上下文的向量，大小为64
		m_datas.resize(64); // 这里只是预分配了64个位置，也就是只创建了指针数组，实际上并没有创建FdCtx对象，后边需要用到时才会创建；
							// 而每个套接字的fd_context和指针数组都是提前创建好的，用到时直接往里该回调函数就行了
	}

	// 获取FdCtx对象
	std::shared_ptr<FdCtx> FdManager::get(int fd, bool auto_create)
	{
		// 如果文件描述符无效，返回nullptr
		if (fd == -1)
		{
			return nullptr;
		}

		// 获取共享锁进行读取操作
		std::shared_lock<std::shared_mutex> read_lock(m_mutex);
		// 如果文件描述符超出范围
		if (m_datas.size() <= fd)
		{
			// 如果不需要自动创建，返回nullptr
			if (auto_create == false)
			{
				return nullptr;
			}
		}
		else
		{
			// 如果存在有效的FdCtx或不需要自动创建，返回对应的FdCtx
			if (m_datas[fd] || !auto_create)
			{
				return m_datas[fd]; // 之前已经创建过了，直接返回
			}
		}

		// 解锁共享锁，获取独占锁进行写操作
		read_lock.unlock();
		std::unique_lock<std::shared_mutex> write_lock(m_mutex);

		// 如果文件描述符超出范围，调整向量大小
		if (m_datas.size() <= fd)
		{
			m_datas.resize(fd * 1.5);
		}

		// 创建新的FdCtx对象并存储在向量中
		m_datas[fd] = std::make_shared<FdCtx>(fd); // 返回新创建的FdCtx对象
		return m_datas[fd]; // 返回新创建的FdCtx对象
	}

	// 删除FdCtx对象
	void FdManager::del(int fd)
	{
		// 获取独占锁进行写操作
		std::unique_lock<std::shared_mutex> write_lock(m_mutex);
		// 如果文件描述符超出范围，直接返回
		if (m_datas.size() <= fd)
		{
			return;
		}
		// 重置对应的FdCtx对象，智能指针，自动注销对象
		m_datas[fd].reset();
	}

} // namespace corlib
