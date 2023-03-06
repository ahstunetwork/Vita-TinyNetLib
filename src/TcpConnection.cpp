//
// Created by vitanmc on 2023/3/5.
//

#include <functional>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

namespace Vita {


    static EventLoop *CheckLoopNotNull(EventLoop *loop) {
        if (loop == nullptr) {
            LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
        }
        return loop;
    }


    // tcpConnection�Ĺ��캯��
    // 1. һ���µ�socket��acceptor accept���ص�connfd��
    // 2. socket���׵�channel
    // 3. localAddr
    // 4. peerAddr
    // 5. ˮλ
    // Ϊsocket�����ĸ��ں˻ص�����
    TcpConnection::TcpConnection(EventLoop *loop,
                                 const std::string &nameArg,
                                 int sockfd,
                                 const InetAddress &localAddr,
                                 const InetAddress &peerAddr)
            : loop_(CheckLoopNotNull(loop)), name_(nameArg), state_(kConnecting), reading_(true),
              socket_(new Socket(sockfd)), channel_(new Channel(loop, sockfd)), localAddr_(localAddr),
              peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024) // 64M
    {
        // �����channel������Ӧ�Ļص����� poller��channel֪ͨ����Ȥ���¼������� channel��ص���Ӧ�Ļص�����
        channel_->setReadCallback(
                std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
        channel_->setWriteCallback(
                std::bind(&TcpConnection::handleWrite, this));
        channel_->setCloseCallback(
                std::bind(&TcpConnection::handleClose, this));
        channel_->setErrorCallback(
                std::bind(&TcpConnection::handleError, this));

        LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
        socket_->setKeepAlive(true);
    }

    TcpConnection::~TcpConnection() {
        LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), channel_->fd(), (int) state_);
    }

    void TcpConnection::send(const std::string &buf) {
        if (state_ == kConnected) {
            if (loop_->isInLoopThread()) // �����Ƕ��ڵ���reactor����� �û�����conn->sendʱ loop_��Ϊ��ǰ�߳�
            {
                sendInLoop(buf.c_str(), buf.size());
            } else {
                loop_->runInLoop(
                        std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
            }
        }
    }

/**
 * �������� Ӧ��д�Ŀ� ���ں˷��������� ��Ҫ�Ѵ���������д�뻺����������������ˮλ�ص�
 **/
    void TcpConnection::sendInLoop(const void *data, size_t len) {
        ssize_t nwrote = 0;
        size_t remaining = len;
        bool faultError = false;

        if (state_ == kDisconnected) // ֮ǰ���ù���connection��shutdown �����ٽ��з�����
        {
            LOG_ERROR("disconnected, give up writing");
        }

        // ��ʾchannel_��һ�ο�ʼд���ݻ��߻�����û�д���������
        if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
            nwrote = ::write(channel_->fd(), data, len);
            if (nwrote >= 0) {
                remaining = len - nwrote;
                if (remaining == 0 && writeCompleteCallback_) {
                    // ��Ȼ����������ȫ��������ɣ��Ͳ����ٸ�channel����epollout�¼���
                    loop_->queueInLoop(
                            std::bind(writeCompleteCallback_, shared_from_this()));
                }
            } else // nwrote < 0
            {
                nwrote = 0;
                if (errno != EWOULDBLOCK) // EWOULDBLOCK��ʾ�����������û�����ݺ���������� ��ͬ��EAGAIN
                {
                    LOG_ERROR("TcpConnection::sendInLoop");
                    if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                    {
                        faultError = true;
                    }
                }
            }
        }
        /**
         * ˵����ǰ��һ��write��û�а�����ȫ�����ͳ�ȥ ʣ���������Ҫ���浽����������
         * Ȼ���channelע��EPOLLOUT�¼���Poller����tcp�ķ��ͻ������пռ���֪ͨ
         * ��Ӧ��sock->channel������channel��Ӧע���writeCallback_�ص�������
         * channel��writeCallback_ʵ���Ͼ���TcpConnection���õ�handleWrite�ص���
         * �ѷ��ͻ�����outputBuffer_������ȫ���������
         **/
        if (!faultError && remaining > 0) {
            // Ŀǰ���ͻ�����ʣ��Ĵ����͵����ݵĳ���
            size_t oldLen = outputBuffer_.readableBytes();
            if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
                loop_->queueInLoop(
                        std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
            }
            outputBuffer_.append((char *) data + nwrote, remaining);
            if (!channel_->isWriting()) {
                channel_->enableWriting(); // ����һ��Ҫע��channel��д�¼� ����poller�����channel֪ͨepollout
            }
        }
    }

    void TcpConnection::shutdown() {
        if (state_ == kConnected) {
            setState(kDisconnecting);
            loop_->runInLoop(
                    std::bind(&TcpConnection::shutdownInLoop, this));
        }
    }

    void TcpConnection::shutdownInLoop() {
        if (!channel_->isWriting()) // ˵����ǰoutputBuffer_������ȫ�����ⷢ�����
        {
            socket_->shutdownWrite();
        }
    }

// ���ӽ���
    void TcpConnection::connectEstablished() {
        setState(kConnected);
        channel_->tie(shared_from_this());
        channel_->enableReading(); // ��pollerע��channel��EPOLLIN���¼�

        // �����ӽ��� ִ�лص�
        connectionCallback_(shared_from_this());
    }

// ��������
    void TcpConnection::connectDestroyed() {
        if (state_ == kConnected) {
            setState(kDisconnected);
            channel_->disableAll(); // ��channel�����и���Ȥ���¼���poller��ɾ����
            connectionCallback_(shared_from_this());
        }
        channel_->remove(); // ��channel��poller��ɾ����
    }

// ������Է��������Ե� ���Զ˿ͻ��������ݵ���
// �������˼�⵽EPOLLIN �ͻᴥ����fd�ϵĻص� handleReadȡ���߶Զ˷���������
    void TcpConnection::handleRead(Timestamp receiveTime) {
        int savedErrno = 0;
        ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
        if (n > 0) // �����ݵ���
        {
            // �ѽ������ӵ��û��пɶ��¼�������
            // �����û�����Ļص�����onMessage
            // shared_from_this���ǻ�ȡ��TcpConnection������ָ��
            messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
        } else if (n == 0) // �ͻ��˶Ͽ�
        {
            handleClose();
        } else // ������
        {
            errno = savedErrno;
            LOG_ERROR("TcpConnection::handleRead");
            handleError();
        }
    }

    void TcpConnection::handleWrite() {
        if (channel_->isWriting()) {
            int savedErrno = 0;
            ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
            if (n > 0) {
                outputBuffer_.retrieve(n);
                if (outputBuffer_.readableBytes() == 0) {
                    channel_->disableWriting();
                    if (writeCompleteCallback_) {
                        // TcpConnection�����������ڵ�subloop�� ��pendingFunctors_�м���ص�
                        loop_->queueInLoop(
                                std::bind(writeCompleteCallback_, shared_from_this()));
                    }
                    if (state_ == kDisconnecting) {
                        shutdownInLoop(); // �ڵ�ǰ������loop�а�TcpConnectionɾ����
                    }
                }
            } else {
                LOG_ERROR("TcpConnection::handleWrite");
            }
        } else {
            LOG_ERROR("TcpConnection fd=%d is down, no more writing", channel_->fd());
        }
    }

    void TcpConnection::handleClose() {
        LOG_INFO("TcpConnection::handleClose fd=%d state=%d\n", channel_->fd(), (int) state_);
        setState(kDisconnected);
        channel_->disableAll();

        TcpConnectionPtr connPtr(shared_from_this());
        connectionCallback_(connPtr); // ִ�����ӹرյĻص�
        closeCallback_(connPtr);      // ִ�йر����ӵĻص� ִ�е���TcpServer::removeConnection�ص�����   // must be the last line
    }

    void TcpConnection::handleError() {
        int optval;
        socklen_t optlen = sizeof optval;
        int err = 0;
        if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
            err = errno;
        } else {
            err = optval;
        }
        LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
    }


} // Vita