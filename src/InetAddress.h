//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_INETADDRESS_H
#define VITANETLIB_INETADDRESS_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

namespace Vita {

// ��װsocket��ַ����
    class InetAddress {
    public:
        explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");

        explicit InetAddress(const sockaddr_in &addr)
                : addr_(addr) {
        }

        std::string toIp() const;
        uint16_t toPort() const;

        std::string toIpPort() const;


        //�ͷ����ڲ��� sockaddr_in
        const sockaddr_in *getSockAddr() const { return &addr_; }

        //ֱ�������ڲ��� addr
        void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

    private:
        sockaddr_in addr_;
    };

} // Vita

#endif //VITANETLIB_INETADDRESS_H
