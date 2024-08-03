#ifndef _FD_MANAGER_H_
#define _FD_MANAGER_H_

#include <memory>		
#include <shared_mutex> 
#include "thread.h"		

namespace corlib
{

	// 文件描述符信息类
	class FdCtx : public std::enable_shared_from_this<FdCtx>
	{
	private:
		bool m_isInit = false;		 // 是否初始化
		bool m_isSocket = false;	 // 是否是socket
		bool m_sysNonblock = false;	 // 系统级非阻塞标志
		bool m_userNonblock = false; // 用户级非阻塞标志
		bool m_isClosed = false;	 // 是否已关闭
		int m_fd;					 // 文件描述符

		// 读取事件超时时间
		uint64_t m_recvTimeout = (uint64_t)-1;
		// 写入事件超时时间
		uint64_t m_sendTimeout = (uint64_t)-1;

	public:
		FdCtx(int fd); // 构造函数
		~FdCtx();	   // 析构函数

		bool init();								 // 初始化
		bool isInit() const { return m_isInit; }	 // 是否初始化
		bool isSocket() const { return m_isSocket; } // 是否是socket
		bool isClosed() const { return m_isClosed; } // 是否已关闭

		void setUserNonblock(bool v) { m_userNonblock = v; }	// 设置用户级非阻塞标志
		bool getUserNonblock() const { return m_userNonblock; } // 获取用户级非阻塞标志

		void setSysNonblock(bool v) { m_sysNonblock = v; }	  // 设置系统级非阻塞标志
		bool getSysNonblock() const { return m_sysNonblock; } // 获取系统级非阻塞标志

		void setTimeout(int type, uint64_t v); // 设置超时时间
		uint64_t getTimeout(int type);		   // 获取超时时间
	};

	// 文件描述符管理类
	class FdManager
	{
	public:
		FdManager(); // 构造函数

		std::shared_ptr<FdCtx> get(int fd, bool auto_create = false); // 获取FdCtx对象 ，如果第一次获取的话，auto_create为true，会创建一个FdCtx对象，否则直接返回nullptr，如果fd大于m_datas的大小，会扩容指针数组大小
		void del(int fd);											  // 删除FdCtx对象

	private:
		std::shared_mutex m_mutex;					 // 共享互斥锁
		std::vector<std::shared_ptr<FdCtx>> m_datas; // 存储FdCtx对象的向量
	};

	// 单例模式模板类
	template <typename T>
	class Singleton
	{
	private:
		static T *instance;		 // 单例实例指针
		static std::mutex mutex; // 互斥锁

	protected:
		Singleton() {} // 保护的构造函数

	public:
		// 禁止复制构造函数和赋值操作符
		Singleton(const Singleton &) = delete;
		Singleton &operator=(const Singleton &) = delete;

		// 获取单例实例
		static T *GetInstance()
		{
			std::lock_guard<std::mutex> lock(mutex); // 确保线程安全
			if (instance == nullptr)
			{
				instance = new T();
			}
			return instance;
		}

		// 销毁单例实例
		static void DestroyInstance()
		{
			std::lock_guard<std::mutex> lock(mutex);
			delete instance;
			instance = nullptr;
		}
	};

	// FdManager单例的类型定义
	typedef Singleton<FdManager> FdMgr;

} // namespace corlib

#endif // _FD_MANAGER_H_
