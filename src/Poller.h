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

// muduo���ж�·�¼��ַ����ĺ���IO����ģ��
    class Poller {
    public:
        using ChannelList = std::vector<Channel *>;

        // Poller�Ĺ��캯��ֻ��Ҫһ��EventLoopָ�룬���ڱ�ʶPoller����һ��loop����
        Poller(EventLoop *loop);

        virtual ~Poller() = default;

        // ������IO���ñ���ͳһ�Ľӿڣ�����poller�����epoll_wait�Ľ��������activeChannels����
        virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;

        //������fd��channel����poller���棬��ʶ��һ�ֹ�ϵĳ��fd
        //���������˭������ã�����channel->updata����
        virtual void updateChannel(Channel *channel) = 0;
        virtual void removeChannel(Channel *channel) = 0;

        // �жϲ���channel�Ƿ��ڵ�ǰ��Poller����
        bool hasChannel(Channel *channel) const;

        // EventLoop����ͨ���ýӿڻ�ȡĬ�ϵ�IO���õľ���ʵ��
        static Poller *newDefaultPoller(EventLoop *loop);

    protected:
        // map��key:sockfd value:sockfd������channelͨ������
        using ChannelMap = std::unordered_map<int, Channel *>;
        ChannelMap channels_;

    private:
        EventLoop *ownerLoop_; // ����Poller�������¼�ѭ��EventLoop
    };


} // Vita

#endif //VITANETLIB_POLLER_H
