//
// Created by vitanmc on 2023/3/5.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

namespace Vita {




    //创建一个非阻塞的 sockfd
    static int createNonblocking() {
        int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
        if (sockfd < 0) {
            LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        }
        return sockfd;
    }

    // acceptor构造流程
    // 1. 持有mainLoop
    // 2. 创建一个新的sockfd
    // 3. 为那个sockfd生成配套的channel
    // 4. 为sockfd绑定一个地址结构
    // 5. 为sockfd设置readCallBack()，由于这个fd只是为了处理新用户连接
    Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
            : loop_(loop), acceptSocket_(createNonblocking()), acceptChannel_(loop, acceptSocket_.fd()),
              listenning_(false) {
        acceptSocket_.setReuseAddr(true);
        acceptSocket_.setReusePort(true);
        acceptSocket_.bindAddress(listenAddr);
        // TcpServer::start() => Acceptor.listen() 如果有新用户连接 要执行一个回调(accept => connfd => 打包成Channel => 唤醒subloop)
        // baseloop监听到有事件发生 => acceptChannel_(listenfd) => 执行该回调函数
        acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
    }

    Acceptor::~Acceptor() {
        acceptChannel_.disableAll();    // 把从Poller中感兴趣的事件删除掉
        acceptChannel_.remove();        // 调用EventLoop->removeChannel => Poller->removeChannel 把Poller的ChannelMap对应的部分删除
    }

    //为新创建的fd设置监听，并且enableReading()
    void Acceptor::listen() {
        listenning_ = true;
        acceptSocket_.listen();         // listen
        acceptChannel_.enableReading(); // acceptChannel_注册至Poller !重要
    }

// listenfd有事件发生了，就是有新用户连接了
    void Acceptor::handleRead() {
        //开一个空地址结构，这个地址结构是对方的，即accept返回的
        InetAddress peerAddr;
        int connfd = acceptSocket_.accept(&peerAddr);
        //NewConnectionCallBack()啥时候设置上的呢？？？？
        if (connfd >= 0) {
            if (NewConnectionCallback_) {
                NewConnectionCallback_(connfd, peerAddr); // 轮询找到subLoop 唤醒并分发当前的新客户端的Channel
            } else {
                ::close(connfd);
            }
        } else {
            LOG_ERROR("%s:%s:%d accept err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
            if (errno == EMFILE) {
                LOG_ERROR("%s:%s:%d sockfd reached limit\n", __FILE__, __FUNCTION__, __LINE__);
            }
        }
    }


} // Vita