//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_ACCEPTOR_H
#define VITANETLIB_ACCEPTOR_H

#include <functional>

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

namespace Vita {

    class EventLoop;

    class InetAddress;

    class Acceptor : NonCopyable {
    public:

        //新连接回调函数
        //和TcpConnection相同的是头需要设置用户态回调函数
        //不同的是TcpConnection四个用户态回调都需要用到
        //而Acceptor只需要用到NewConnectionCallback，而且是Acceptor类自带的
        //Acceptor处理的是连接事件，所以不可能改变的
        using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

        //Acceptor构造需要一个mainLoop和一个地址结构
        Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);

        ~Acceptor();

        void setNewConnectionCallback(const NewConnectionCallback &cb) { NewConnectionCallback_ = cb; }

        bool listenning() const { return listenning_; }

        void listen();

    private:
        void handleRead();

        // Acceptor用的就是用户定义的那个baseLoop 也称作mainLoop
        EventLoop *loop_;
        Socket acceptSocket_;
        Channel acceptChannel_;
        NewConnectionCallback NewConnectionCallback_;
        bool listenning_;
    };

} // Vita

#endif //VITANETLIB_ACCEPTOR_H
