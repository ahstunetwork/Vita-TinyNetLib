//
// Created by vitanmc on 2023/3/6.
//

#include <functional>
#include <string.h>

#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

namespace Vita {

    static EventLoop *CheckLoopNotNull(EventLoop *loop) {
        if (loop == nullptr) {
            LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
        }
        return loop;
    }




    //TcpServer�Ĺ��캯��
    // 1. loop���ĸ�baseLoop
    // 2. Acceptor���½���Acceptor
    //      2.1 ����baseLoop
    //      2.2 ����listenAddr
    // 3. ThreadPool���½���EventLoopThreadPool
    //      3.1 ����baseLoop
    // 4. acceptor����newConnectionCallback

    TcpServer::TcpServer(EventLoop *loop,
                         const InetAddress &listenAddr,
                         const std::string &nameArg,
                         Option option)
            : loop_(CheckLoopNotNull(loop)), ipPort_(listenAddr.toIpPort()), name_(nameArg),
              acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
              threadPool_(new EventLoopThreadPool(loop, name_)), connectionCallback_(), messageCallback_(),
              nextConnId_(1), started_(0) {
        // �������û�����ʱ��Acceptor���а󶨵�acceptChannel_���ж��¼�����
        // ִ��handleRead()
        // handleRead()�������ڻ����TcpServer::newConnection�ص�
        acceptor_->setNewConnectionCallback(
                std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
    }

    TcpServer::~TcpServer() {
        for (auto &item: connections_) {
            TcpConnectionPtr conn(item.second);
            item.second.reset();    // ��ԭʼ������ָ�븴λ ��ջ�ռ��TcpConnectionPtr connָ��ö��� ��conn������������ �����ͷ�����ָ��ָ��Ķ���
            // ��������
            conn->getLoop()->runInLoop(
                    std::bind(&TcpConnection::connectDestroyed, conn));
        }
    }

// ���õײ�subloop�ĸ���
    void TcpServer::setThreadNum(int numThreads) {
        threadPool_->setThreadNum(numThreads);
    }

// ��������������
    void TcpServer::start() {
        if (started_++ == 0)    // ��ֹһ��TcpServer����start���
        {
            threadPool_->start(threadInitCallback_);    // �����ײ��loop�̳߳�
            loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
        }
    }

// ��һ�����û����ӣ�acceptor��ִ������ص�����
// acceptor�ᴴ�� �µ�fd����װ��channel��enableReading
// �������ӵ�ʱ��acceptorfd����������handleRead()����һ������
// acceptor��������һ���µ�fd�������µĵ�ַ�ṹ(�Զ˵�peerAddr)
// Ȼ��acceptor����newConnection => TcpConnection => ��ѯ
// ����mainLoop���յ�����������(acceptChannel_���ж��¼�����)ͨ���ص���ѯ�ַ���subLoopȥ����


// peerAddrΪAcceptor����accept������
// localAddrΪ�����󶨵�
    void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
        // ��ѯ�㷨 ѡ��һ��subLoop ������ connfd ��Ӧ�� channel
        EventLoop *ioLoop = threadPool_->getNextLoop();
        char buf[64] = {0};
        snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
        ++nextConnId_;  // ����û������Ϊԭ��������Ϊ��ֻ��mainloop��ִ�� ���漰�̰߳�ȫ����
        std::string connName = name_ + buf;

        LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
                 name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

        // ͨ��sockfd��ȡ��󶨵ı�����ip��ַ�Ͷ˿���Ϣ
        sockaddr_in local;
        ::memset(&local, 0, sizeof(local));
        socklen_t addrlen = sizeof(local);
        if (::getsockname(sockfd, (sockaddr *) &local, &addrlen) < 0) {
            LOG_ERROR("sockets::getLocalAddr");
        }

        InetAddress localAddr(local);


        // �½�һ��TcpConnection������������զacceptor������ɣ�
        // TcpConnection����
        // 1. ioLoop��EventLoopTHreadPool����ѯȡ����
        // 2. sockfd��Acceptorֱ��bind��
        // 3. localAddr
        // 4. peerAddr
        TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                                connName,
                                                sockfd,
                                                localAddr,
                                                peerAddr));
        connections_[connName] = conn;
        // ����Ļص������û����ø�TcpServer => TcpConnection��
        // ����Channel�󶨵�����TcpConnection���õ��ĸ���handleRead,handleWrite...
        // ������Ļص�����handlexxx������
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setWriteCompleteCallback(writeCompleteCallback_);

        // ��������ιر����ӵĻص�
        conn->setCloseCallback(
                std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

        ioLoop->runInLoop(
                std::bind(&TcpConnection::connectEstablished, conn));
    }

    void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
        loop_->runInLoop(
                std::bind(&TcpServer::removeConnectionInLoop, this, conn));
    }

    void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
        LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
                 name_.c_str(), conn->name().c_str());

        connections_.erase(conn->name());
        EventLoop *ioLoop = conn->getLoop();
        ioLoop->queueInLoop(
                std::bind(&TcpConnection::connectDestroyed, conn));
    }


} // Vita