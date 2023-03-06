//
// Created by vitanmc on 2023/3/6.
//

#ifndef VITANETLIB_TCPSERVER_H
#define VITANETLIB_TCPSERVER_H

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

namespace Vita {

/**
 * 用户使用muduo编写服务器程序
 **/


    // 对外的服务器编程使用的类
    class TcpServer {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        enum Option {
            kNoReusePort,
            kReusePort,
        };

        //TcpServer的构造函数需要一个EventLoop，一个ip地址结构
        TcpServer(EventLoop *loop,
                  const InetAddress &listenAddr,
                  const std::string &nameArg,
                  Option option = kNoReusePort);

        ~TcpServer();

        void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }

        void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }

        void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

        void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

        // 设置底层subloop的个数
        void setThreadNum(int numThreads);

        // 开启服务器监听
        void start();

    private:
        //创建一个TcpConnection，参数为
        // 1. Acceptor返回的sockfd
        // 2. 对方的ip地址结构
        void newConnection(int sockfd, const InetAddress &peerAddr);

        void removeConnection(const TcpConnectionPtr &conn);

        void removeConnectionInLoop(const TcpConnectionPtr &conn);


        // TcpServer持有一个map\name--TcpConnectionPtr
        using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

        // Tcp持有原始的baseLoop
        EventLoop *loop_;

        const std::string ipPort_;
        const std::string name_;

        std::unique_ptr<Acceptor> acceptor_; // 运行在mainloop 任务就是监听新连接事件

        //持有所有的 EventLoopThread
        std::shared_ptr<EventLoopThreadPool> threadPool_;

        ThreadInitCallback threadInitCallback_; // loop线程初始化的回调
        ConnectionCallback connectionCallback_;       //有新连接时的回调
        MessageCallback messageCallback_;             // 有读写事件发生时的回调
        WriteCompleteCallback writeCompleteCallback_; // 消息发送完成后的回调


        std::atomic_int started_;

        int nextConnId_;
        ConnectionMap connections_; // 保存所有的连接
    };

} // Vita

#endif //VITANETLIB_TCPSERVER_H
