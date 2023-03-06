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
 * epoll的使用:
 * 1. epoll_create
 * 2. epoll_ctl (add, mod, del)
 * 3. epoll_wait
 **/

    class Channel;

    class EPollPoller : public Poller {
    public:
        //EPollPoller和poller一样，构造函数只需要一个EventLoop
        EPollPoller(EventLoop *loop);
        ~EPollPoller() override;

        // 重写基类Poller的抽象方法
        Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
        void updateChannel(Channel *channel) override;
        void removeChannel(Channel *channel) override;
        // 填写活跃的连接
        void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

        // 更新channel通道 其实就是调用epoll_ctl
        void update(int operation, Channel *channel);

    private:
        //fillEventListSize为epoll_wait传出的数组，由于我们使用vector来坐的，
        // 所以需要一个初始大小，来resize
        static const int kInitEventListSize = 16;
        using EventList = std::vector<epoll_event>; // C++中可以省略struct 直接写epoll_event即可
        int epollfd_;      // epoll_create创建返回的fd保存在epollfd_中
        EventList events_; // 用于存放epoll_wait返回的所有发生的事件的文件描述符事件集
    };

} // Vita

#endif //VITANETLIB_EPOLLPOLLER_H
