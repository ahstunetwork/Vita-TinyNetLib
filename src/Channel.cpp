//
// Created by vitanmc on 2023/3/5.
//

#include <sys/epoll.h>
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

namespace Vita {

    const int Channel::kNoneEvent = 0;
    const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
    const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop: ChannelList Poller
    Channel::Channel(EventLoop *loop, int fd)
            : loop_(loop), fd_(fd), events_(0), revents_(0), status_(-1), tied_(false) {
    }

    Channel::~Channel() {
    }

// channel��tie����ʲôʱ����ù�?  TcpConnection => channel
/**
 * TcpConnection��ע����Chnanel��Ӧ�Ļص�����������Ļص�������ΪTcpConnection
 * ����ĳ�Ա��������˿���˵��һ����ǣ�Channel�Ľ���һ������TcpConnection����
 * �˴���tieȥ���TcoConnection��Channel����������ʱ�����⣬�Ӷ���֤��Channel��
 * ���ܹ���TcpConnection����ǰ���١�
 **/
    void Channel::tie(const std::shared_ptr<void> &obj) {
        tie_ = obj;
        tied_ = true;
    }

/**
 * ���ı�channel����ʾ��fd��events�¼���update������poller�������fd��Ӧ���¼�epoll_ctl
 **/
    void Channel::update() {
        // ͨ��channel������eventloop������poller����Ӧ������ע��fd��events�¼�
        loop_->updateChannel(this);
    }

// ��channel������EventLoop�аѵ�ǰ��channelɾ����
    void Channel::remove() {
        loop_->removeChannel(this);
    }



    //handleEvent�����¼����ںδ���������?
    void Channel::handleEvent(Timestamp receiveTime) {
        if (tied_) {
            std::shared_ptr<void> guard = tie_.lock();
            if (guard) {
                handleEventWithGuard(receiveTime);
            }
            // �������ʧ���� �Ͳ����κδ��� ˵��Channel��TcpConnection�����Ѿ���������
        } else {
            handleEventWithGuard(receiveTime);
        }
    }

    //�ж����fd����ϵ���¼���ʵ�ʷ������¼������ݷ����¼��Ĳ�ͬ�����ò�ͬ�Ļص�����
    void Channel::handleEventWithGuard(Timestamp receiveTime) {
        LOG_INFO("channel handleEvent revents:%d\n", revents_);
        // �ر�
        if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) // ��TcpConnection��ӦChannel ͨ��shutdown �ر�д�� epoll����EPOLLHUP
        {
            if (closeCallback_) {
                closeCallback_();
            }
        }
        // ����
        if (revents_ & EPOLLERR) {
            if (errorCallback_) {
                errorCallback_();
            }
        }
        // ��
        if (revents_ & (EPOLLIN | EPOLLPRI)) {
            if (readCallback_) {
                readCallback_(receiveTime);
            }
        }
        // д
        if (revents_ & EPOLLOUT) {
            if (writeCallback_) {
                writeCallback_();
            }
        }
    }


} // Vita