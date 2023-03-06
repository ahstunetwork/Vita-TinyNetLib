//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_TCPCONNECTION_H
#define VITANETLIB_TCPCONNECTION_H


#include <memory>
#include <string>
#include <atomic>

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

namespace Vita {


    class Channel;

    class EventLoop;

    class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection设置回调 => 设置到Channel => Poller => Channel回调
 **/

    class TcpConnection : NonCopyable, public std::enable_shared_from_this<TcpConnection> {
    public:
        TcpConnection(EventLoop *loop,
                      const std::string &nameArg,
                      int sockfd,
                      const InetAddress &localAddr,
                      const InetAddress &peerAddr);

        ~TcpConnection();

        EventLoop *getLoop() const { return loop_; }

        const std::string &name() const { return name_; }

        const InetAddress &localAddress() const { return localAddr_; }

        const InetAddress &peerAddress() const { return peerAddr_; }

        bool connected() const { return state_ == kConnected; }

        // 发送数据
        void send(const std::string &buf);

        // 关闭连接
        void shutdown();

        void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
        void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
        void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
        void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
        void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
            highWaterMarkCallback_ = cb;
            highWaterMark_ = highWaterMark;
        }

        // 连接建立
        void connectEstablished();

        // 连接销毁
        void connectDestroyed();

    private:
        enum StateE {
            kDisconnected, // 已经断开连接
            kConnecting,   // 正在连接
            kConnected,    // 已连接
            kDisconnecting // 正在断开连接
        };

        void setState(StateE state) { state_ = state; }


        //这四个是fd上的干活用的回调函数
        void handleRead(Timestamp receiveTime);
        void handleWrite();
        void handleClose();
        void handleError();

        void sendInLoop(const void *data, size_t len);
        void shutdownInLoop();

        // 这里是baseloop还是subloop由TcpServer中创建的线程数决定
        // 若为多Reactor 该loop_指向subloop 若为单Reactor 该loop_指向baseloop
        EventLoop *loop_;
        const std::string name_;
        std::atomic_int state_;
        bool reading_;

        // Socket Channel 这里和Acceptor类似
        // Acceptor => mainloop
        // TcpConnection => subloop
        std::unique_ptr<Socket> socket_;
        std::unique_ptr<Channel> channel_;

        const InetAddress localAddr_;
        const InetAddress peerAddr_;

        // 这些回调TcpServer也有 用户通过写入TcpServer注册
        // TcpServer再将注册的回调传递给TcpConnection
        // TcpConnection再将回调注册到Channel中
        // 这一组回调函数是用户态的，前面那一组HandleRead,HeadleWrite内核态的
        ConnectionCallback connectionCallback_;       // 有新连接时的回调
        MessageCallback messageCallback_;             // 有读写消息时的回调
        WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
        HighWaterMarkCallback highWaterMarkCallback_;
        CloseCallback closeCallback_;
        size_t highWaterMark_;

        // 数据缓冲区
        Buffer inputBuffer_;    // 接收数据的缓冲区
        Buffer outputBuffer_;   // 发送数据的缓冲区 用户send向outputBuffer_发
    };


} // Vita

#endif //VITANETLIB_TCPCONNECTION_H
