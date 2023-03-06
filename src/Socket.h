//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_SOCKET_H
#define VITANETLIB_SOCKET_H

#include "noncopyable.h"

namespace Vita {

#pragma once


    class InetAddress;

// 封装socket fd
    class Socket : NonCopyable {
    public:
        explicit Socket(int sockfd)
                : sockfd_(sockfd) {}

        ~Socket();


        //得到内部 fd
        int fd() const { return sockfd_; }
        //绑定地址结构，绑定地址结构是自己fd需要绑定的
        void bindAddress(const InetAddress &localaddr);

        void listen();
        //accept函数调用也需要一个地址结构，这个地址结构是传出的，是对方的
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
