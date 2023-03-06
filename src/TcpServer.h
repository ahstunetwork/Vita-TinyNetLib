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
 * �û�ʹ��muduo��д����������
 **/


    // ����ķ��������ʹ�õ���
    class TcpServer {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        enum Option {
            kNoReusePort,
            kReusePort,
        };

        //TcpServer�Ĺ��캯����Ҫһ��EventLoop��һ��ip��ַ�ṹ
        TcpServer(EventLoop *loop,
                  const InetAddress &listenAddr,
                  const std::string &nameArg,
                  Option option = kNoReusePort);

        ~TcpServer();

        void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }

        void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }

        void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

        void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

        // ���õײ�subloop�ĸ���
        void setThreadNum(int numThreads);

        // ��������������
        void start();

    private:
        //����һ��TcpConnection������Ϊ
        // 1. Acceptor���ص�sockfd
        // 2. �Է���ip��ַ�ṹ
        void newConnection(int sockfd, const InetAddress &peerAddr);

        void removeConnection(const TcpConnectionPtr &conn);

        void removeConnectionInLoop(const TcpConnectionPtr &conn);


        // TcpServer����һ��map\name--TcpConnectionPtr
        using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

        // Tcp����ԭʼ��baseLoop
        EventLoop *loop_;

        const std::string ipPort_;
        const std::string name_;

        std::unique_ptr<Acceptor> acceptor_; // ������mainloop ������Ǽ����������¼�

        //�������е� EventLoopThread
        std::shared_ptr<EventLoopThreadPool> threadPool_;

        ThreadInitCallback threadInitCallback_; // loop�̳߳�ʼ���Ļص�
        ConnectionCallback connectionCallback_;       //��������ʱ�Ļص�
        MessageCallback messageCallback_;             // �ж�д�¼�����ʱ�Ļص�
        WriteCompleteCallback writeCompleteCallback_; // ��Ϣ������ɺ�Ļص�


        std::atomic_int started_;

        int nextConnId_;
        ConnectionMap connections_; // �������е�����
    };

} // Vita

#endif //VITANETLIB_TCPSERVER_H
