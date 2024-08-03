#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <atomic>

#define SERVER_IP "127.0.0.1"    // 服务器IP地址
#define SERVER_PORT 8080         // 服务器端口号
#define MESSAGE "Hello, Server!" // 要发送的消息
#define NUM_THREADS 100        // 线程数

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

// 全局原子变量用于统计总的请求数量
std::atomic<int> total_requests(0);

void client_task()
{
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[1024];

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        error("Error opening socket");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        error("Invalid address/ Address not supported");
    }

    // 连接服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        error("Connection Failed");
    }

    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < end_time)
    {
        // 发送消息
        int sent_bytes = send(sockfd, MESSAGE, strlen(MESSAGE), 0);
        if (sent_bytes < 0)
        {
            error("Error sending message");
        }

        // 接收回复
        int received_bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (received_bytes < 0)
        {
            error("Error receiving message");
        }

        buffer[received_bytes] = '\0'; // 确保消息以空字符结尾
        std::cout << "Received from server: " << buffer << std::endl;

        // 增加请求计数
        total_requests++;
    }

    // 关闭套接字
    close(sockfd);
}

int main()
{
    std::vector<std::thread> threads;

    // 创建1000个线程
    for (int i = 0; i < NUM_THREADS; ++i)
    {
        threads.emplace_back(client_task);
    }

    // 等待所有线程完成
    for (auto &t : threads)
    {
        t.join();
    }

    // 输出每秒请求数
    std::cout << "Total requests: " << total_requests << std::endl;
    std::cout << "Requests per second: " << total_requests / 60.0 << std::endl;

    return 0;
}
