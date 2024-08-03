#ifndef _HOOK_H_
#define _HOOK_H_

#include <unistd.h>		// 包含UNIX标准头文件，提供sleep、usleep等函数
#include <sys/socket.h> // 包含socket相关头文件，提供socket、connect、accept等函数
#include <sys/types.h>	// 包含系统类型头文件，提供基本数据类型
#include <sys/uio.h>	// 包含向量I/O操作头文件，提供readv、writev等函数
#include <sys/ioctl.h>	// 包含ioctl系统调用头文件，提供ioctl函数
#include <fcntl.h>		// 包含文件控制操作头文件，提供fcntl函数

namespace corlib
{

	// 检查钩子是否启用
	bool is_hook_enable();

	// 设置钩子启用状态
	void set_hook_enable(bool flag);

} // namespace corlib

extern "C"
{
	// 函数指针类型定义，指向原始函数
	typedef unsigned int (*sleep_fun)(unsigned int seconds);
	extern sleep_fun sleep_f; // 声明原始sleep函数的函数指针

	typedef int (*usleep_fun)(useconds_t usec);
	extern usleep_fun usleep_f; // 声明原始usleep函数的函数指针

	typedef int (*nanosleep_fun)(const struct timespec *req, struct timespec *rem);
	extern nanosleep_fun nanosleep_f; // 声明原始nanosleep函数的函数指针

	typedef int (*socket_fun)(int domain, int type, int protocol);
	extern socket_fun socket_f; // 声明原始socket函数的函数指针

	typedef int (*connect_fun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	extern connect_fun connect_f; // 声明原始connect函数的函数指针

	typedef int (*accept_fun)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	extern accept_fun accept_f; // 声明原始accept函数的函数指针

	typedef ssize_t (*read_fun)(int fd, void *buf, size_t count);
	extern read_fun read_f; // 声明原始read函数的函数指针

	typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
	extern readv_fun readv_f; // 声明原始readv函数的函数指针

	typedef ssize_t (*recv_fun)(int sockfd, void *buf, size_t len, int flags);
	extern recv_fun recv_f; // 声明原始recv函数的函数指针

	typedef ssize_t (*recvfrom_fun)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
	extern recvfrom_fun recvfrom_f; // 声明原始recvfrom函数的函数指针

	typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr *msg, int flags);
	extern recvmsg_fun recvmsg_f; // 声明原始recvmsg函数的函数指针

	typedef ssize_t (*write_fun)(int fd, const void *buf, size_t count);
	extern write_fun write_f; // 声明原始write函数的函数指针

	typedef ssize_t (*writev_fun)(int fd, const struct iovec *iov, int iovcnt);
	extern writev_fun writev_f; // 声明原始writev函数的函数指针

	typedef ssize_t (*send_fun)(int sockfd, const void *buf, size_t len, int flags);
	extern send_fun send_f; // 声明原始send函数的函数指针

	typedef ssize_t (*sendto_fun)(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
	extern sendto_fun sendto_f; // 声明原始sendto函数的函数指针

	typedef ssize_t (*sendmsg_fun)(int sockfd, const struct msghdr *msg, int flags);
	extern sendmsg_fun sendmsg_f; // 声明原始sendmsg函数的函数指针

	typedef int (*close_fun)(int fd);
	extern close_fun close_f; // 声明原始close函数的函数指针

	typedef int (*fcntl_fun)(int fd, int cmd, ... /* arg */);
	extern fcntl_fun fcntl_f; // 声明原始fcntl函数的函数指针

	typedef int (*ioctl_fun)(int fd, unsigned long request, ...);
	extern ioctl_fun ioctl_f; // 声明原始ioctl函数的函数指针

	typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
	extern getsockopt_fun getsockopt_f; // 声明原始getsockopt函数的函数指针

	typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
	extern setsockopt_fun setsockopt_f; // 声明原始setsockopt函数的函数指针

	// 函数原型声明，用于覆盖系统函数
	// sleep函数
	unsigned int sleep(unsigned int seconds);
	int usleep(useconds_t usec);
	int nanosleep(const struct timespec *req, struct timespec *rem);

	// socket函数
	int socket(int domain, int type, int protocol);
	int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
	int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

	// 读操作
	ssize_t read(int fd, void *buf, size_t count);
	ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
	ssize_t recv(int sockfd, void *buf, size_t len, int flags);
	ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
	ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

	// 写操作
	ssize_t write(int fd, const void *buf, size_t count);
	ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
	ssize_t send(int sockfd, const void *buf, size_t len, int flags);
	ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
	ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);

	// 文件描述符操作
	int close(int fd);

	// socket控制
	int fcntl(int fd, int cmd, ... /* arg */);
	int ioctl(int fd, unsigned long request, ...);
	int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
	int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
}

#endif // _HOOK_H_
