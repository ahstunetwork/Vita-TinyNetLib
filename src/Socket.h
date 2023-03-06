//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_SOCKET_H
#define VITANETLIB_SOCKET_H

#include "noncopyable.h"

namespace Vita {

#pragma once


    class InetAddress;

// ��װsocket fd
    class Socket : NonCopyable {
    public:
        explicit Socket(int sockfd)
                : sockfd_(sockfd) {}

        ~Socket();


        //�õ��ڲ� fd
        int fd() const { return sockfd_; }
        //�󶨵�ַ�ṹ���󶨵�ַ�ṹ���Լ�fd��Ҫ�󶨵�
        void bindAddress(const InetAddress &localaddr);

        void listen();
        //accept��������Ҳ��Ҫһ����ַ�ṹ�������ַ�ṹ�Ǵ����ģ��ǶԷ���
        int accept(InetAddress *peeraddr);

        void shutdownWrite();

        void setTcpNoDelay(bool on);

        void setReuseAddr(bool on);

        void setReusePort(bool on);

        void setKeepAlive(bool on);

    private:
        const int sockfd_;
    };


} // Vita

#endif //VITANETLIB_SOCKET_H
