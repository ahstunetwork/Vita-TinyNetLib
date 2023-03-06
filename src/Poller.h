//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_POLLER_H
#define VITANETLIB_POLLER_H


#include <vector>
#include <unordered_map>

#include "noncopyable.h"
#include "Timestamp.h"

namespace Vita {

    class Channel;
    class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
    class Poller {
    public:
        using ChannelList = std::vector<Channel *>;

        // Poller的构造函数只需要一个EventLoop指针，用于标识Poller在哪一个loop里面
        Poller(EventLoop *loop);

        virtual ~Poller() = default;

        // 给所有IO复用保留统一的接口，调用poller，会把epoll_wait的结果保持在activeChannels里面
        virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;

        //将绑定了fd的channel放入poller里面，标识下一轮关系某个fd
        //这个函数有谁负责调用？是由channel->updata操作
        virtual void updateChannel(Channel *channel) = 0;
        virtual void removeChannel(Channel *channel) = 0;

        // 判断参数channel是否在当前的Poller当中
        bool hasChannel(Channel *channel) const;

        // EventLoop可以通过该接口获取默认的IO复用的具体实现
        static Poller *newDefaultPoller(EventLoop *loop);

    protected:
        // map的key:sockfd value:sockfd所属的channel通道类型
        using ChannelMap = std::unordered_map<int, Channel *>;
        ChannelMap channels_;

    private:
        EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop
    };


} // Vita

#endif //VITANETLIB_POLLER_H
