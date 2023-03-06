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
 * TcpServer => Acceptor => ��һ�����û����ӣ�ͨ��accept�����õ�connfd
 * => TcpConnection���ûص� => ���õ�Channel => Poller => Channel�ص�
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

        // ��������
        void send(const std::string &buf);

        // �ر�����
        void shutdown();

        void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
        void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
        void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
        void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
        void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
            highWaterMarkCallback_ = cb;
            highWaterMark_ = highWaterMark;
        }

        // ���ӽ���
        void connectEstablished();

        // ��������
        void connectDestroyed();

    private:
        enum StateE {
            kDisconnected, // �Ѿ��Ͽ�����
            kConnecting,   // ��������
            kConnected,    // ������
            kDisconnecting // ���ڶϿ�����
        };

        void setState(StateE state) { state_ = state; }


        //���ĸ���fd�ϵĸɻ��õĻص�����
        void handleRead(Timestamp receiveTime);
        void handleWrite();
        void handleClose();
        void handleError();

        void sendInLoop(const void *data, size_t len);
        void shutdownInLoop();

        // ������baseloop����subloop��TcpServer�д������߳�������
        // ��Ϊ��Reactor ��loop_ָ��subloop ��Ϊ��Reactor ��loop_ָ��baseloop
        EventLoop *loop_;
        const std::string name_;
        std::atomic_int state_;
        bool reading_;

        // Socket Channel �����Acceptor����
        // Acceptor => mainloop
        // TcpConnection => subloop
        std::unique_ptr<Socket> socket_;
        std::unique_ptr<Channel> channel_;

        const InetAddress localAddr_;
        const InetAddress peerAddr_;

        // ��Щ�ص�TcpServerҲ�� �û�ͨ��д��TcpServerע��
        // TcpServer�ٽ�ע��Ļص����ݸ�TcpConnection
        // TcpConnection�ٽ��ص�ע�ᵽChannel��
        // ��һ��ص��������û�̬�ģ�ǰ����һ��HandleRead,HeadleWrite�ں�̬��
        ConnectionCallback connectionCallback_;       // ��������ʱ�Ļص�
        MessageCallback messageCallback_;             // �ж�д��Ϣʱ�Ļص�
        WriteCompleteCallback writeCompleteCallback_; // ��Ϣ��������Ժ�Ļص�
        HighWaterMarkCallback highWaterMarkCallback_;
        CloseCallback closeCallback_;
        size_t highWaterMark_;

        // ���ݻ�����
        Buffer inputBuffer_;    // �������ݵĻ�����
        Buffer outputBuffer_;   // �������ݵĻ����� �û�send��outputBuffer_��
    };


} // Vita

#endif //VITANETLIB_TCPCONNECTION_H
