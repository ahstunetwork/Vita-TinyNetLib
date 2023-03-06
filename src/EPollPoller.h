//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_EPOLLPOLLER_H
#define VITANETLIB_EPOLLPOLLER_H

#include <vector>
#include <sys/epoll.h>

#include "Poller.h"
#include "Timestamp.h"

namespace Vita {


/**
 * epoll��ʹ��:
 * 1. epoll_create
 * 2. epoll_ctl (add, mod, del)
 * 3. epoll_wait
 **/

    class Channel;

    class EPollPoller : public Poller {
    public:
        //EPollPoller��pollerһ�������캯��ֻ��Ҫһ��EventLoop
        EPollPoller(EventLoop *loop);
        ~EPollPoller() override;

        // ��д����Poller�ĳ��󷽷�
        Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
        void updateChannel(Channel *channel) override;
        void removeChannel(Channel *channel) override;
        // ��д��Ծ������
        void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

        // ����channelͨ�� ��ʵ���ǵ���epoll_ctl
        void update(int operation, Channel *channel);

    private:
        //fillEventListSizeΪepoll_wait���������飬��������ʹ��vector�����ģ�
        // ������Ҫһ����ʼ��С����resize
        static const int kInitEventListSize = 16;
        using EventList = std::vector<epoll_event>; // C++�п���ʡ��struct ֱ��дepoll_event����
        int epollfd_;      // epoll_create�������ص�fd������epollfd_��
        EventList events_; // ���ڴ��epoll_wait���ص����з������¼����ļ��������¼���
    };

} // Vita

#endif //VITANETLIB_EPOLLPOLLER_H
