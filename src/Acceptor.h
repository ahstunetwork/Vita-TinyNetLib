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

        //�����ӻص�����
        //��TcpConnection��ͬ����ͷ��Ҫ�����û�̬�ص�����
        //��ͬ����TcpConnection�ĸ��û�̬�ص�����Ҫ�õ�
        //��Acceptorֻ��Ҫ�õ�NewConnectionCallback��������Acceptor���Դ���
        //Acceptor������������¼������Բ����ܸı��
        using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

        //Acceptor������Ҫһ��mainLoop��һ����ַ�ṹ
        Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);

        ~Acceptor();

        void setNewConnectionCallback(const NewConnectionCallback &cb) { NewConnectionCallback_ = cb; }

        bool listenning() const { return listenning_; }

        void listen();

    private:
        void handleRead();

        // Acceptor�õľ����û�������Ǹ�baseLoop Ҳ����mainLoop
        EventLoop *loop_;
        Socket acceptSocket_;
        Channel acceptChannel_;
        NewConnectionCallback NewConnectionCallback_;
        bool listenning_;
    };

} // Vita

#endif //VITANETLIB_ACCEPTOR_H
