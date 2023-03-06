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




    //����һ���������� sockfd
    static int createNonblocking() {
        int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
        if (sockfd < 0) {
            LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        }
        return sockfd;
    }

    // acceptor��������
    // 1. ����mainLoop
    // 2. ����һ���µ�sockfd
    // 3. Ϊ�Ǹ�sockfd�������׵�channel
    // 4. Ϊsockfd��һ����ַ�ṹ
    // 5. Ϊsockfd����readCallBack()���������fdֻ��Ϊ�˴������û�����
    Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
            : loop_(loop), acceptSocket_(createNonblocking()), acceptChannel_(loop, acceptSocket_.fd()),
              listenning_(false) {
        acceptSocket_.setReuseAddr(true);
        acceptSocket_.setReusePort(true);
        acceptSocket_.bindAddress(listenAddr);
        // TcpServer::start() => Acceptor.listen() ��������û����� Ҫִ��һ���ص�(accept => connfd => �����Channel => ����subloop)
        // baseloop���������¼����� => acceptChannel_(listenfd) => ִ�иûص�����
        acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
    }

    Acceptor::~Acceptor() {
        acceptChannel_.disableAll();    // �Ѵ�Poller�и���Ȥ���¼�ɾ����
        acceptChannel_.remove();        // ����EventLoop->removeChannel => Poller->removeChannel ��Poller��ChannelMap��Ӧ�Ĳ���ɾ��
    }

    //Ϊ�´�����fd���ü���������enableReading()
    void Acceptor::listen() {
        listenning_ = true;
        acceptSocket_.listen();         // listen
        acceptChannel_.enableReading(); // acceptChannel_ע����Poller !��Ҫ
    }

// listenfd���¼������ˣ����������û�������
    void Acceptor::handleRead() {
        //��һ���յ�ַ�ṹ�������ַ�ṹ�ǶԷ��ģ���accept���ص�
        InetAddress peerAddr;
        int connfd = acceptSocket_.accept(&peerAddr);
        //NewConnectionCallBack()ɶʱ�������ϵ��أ�������
        if (connfd >= 0) {
            if (NewConnectionCallback_) {
                NewConnectionCallback_(connfd, peerAddr); // ��ѯ�ҵ�subLoop ���Ѳ��ַ���ǰ���¿ͻ��˵�Channel
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