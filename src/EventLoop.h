//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_EVENTLOOP_H
#define VITANETLIB_EVENTLOOP_H

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

namespace Vita {

    class Channel;

    class Poller;

// �¼�ѭ���� ��Ҫ������������ģ�� Channel Poller(epoll�ĳ���)
    class EventLoop : NonCopyable {
    public:
        using Functor = std::function<void()>;

        // Eventloop����ܼ򵥣������κβ�����������loop
        EventLoop();
        ~EventLoop();

        // �����¼�ѭ��
        void loop();

        // �˳��¼�ѭ��
        void quit();

        Timestamp pollReturnTime() const { pollRetureTime_; }

        // �ڵ�ǰloop��ִ��
        void runInLoop(Functor cb);

        // ���ϲ�ע��Ļص�����cb��������� ����loop���ڵ��߳�ִ��cb
        void queueInLoop(Functor cb);

        // ͨ��eventfd����loop���ڵ��߳�
        void wakeup();

        // EventLoop�ķ��� => Poller�ķ���
        // ��channel�򹤵ĺ���
        void updateChannel(Channel *channel);
        void removeChannel(Channel *channel);
        bool hasChannel(Channel *channel);

        // �ж�EventLoop�����Ƿ����Լ����߳���
        bool isInLoopThread() const {
            return threadId_ == CurrentThread::tid();
        } // threadId_ΪEventLoop����ʱ���߳�id CurrentThread::tid()Ϊ��ǰ�߳�id

    private:
        // ��eventfd���ص��ļ�������wakeupFd_�󶨵��¼��ص�
        // ��wakeup()ʱ �����¼�����ʱ ����handleRead()��wakeupFd_��8�ֽ� ͬʱ����������epoll_wait
        void handleRead();
        void doPendingFunctors(); // ִ���ϲ�ص�


        std::atomic_bool looping_; // ԭ�Ӳ��� �ײ�ͨ��CASʵ��
        std::atomic_bool quit_;    // ��ʶ�˳�loopѭ��

        const pid_t threadId_; // ��¼��ǰEventLoop�Ǳ��ĸ��߳�id������ ����ʶ�˵�ǰEventLoop�������߳�id

        Timestamp pollRetureTime_; // Poller���ط����¼���Channels��ʱ���
        std::unique_ptr<Poller> poller_; // Poller��Ҫ��Ͻ�EventLoop����

        // ���ã���mainLoop��ȡһ�����û���Channel
        // ��ͨ����ѯ�㷨ѡ��һ��subLoop ͨ���ó�Ա����subLoop����Channel
        int wakeupFd_;
        std::unique_ptr<Channel> wakeupChannel_;

        //��¼Poller��⵽��ǰ���¼�����������Channel�б�
        //��poller������
        using ChannelList = std::vector<Channel *>;
        ChannelList activeChannels_;

        std::atomic_bool callingPendingFunctors_; // ��ʶ��ǰloop�Ƿ�����Ҫִ�еĻص�����
        std::vector<Functor> pendingFunctors_;    // �洢loop��Ҫִ�е����лص�����
        std::mutex mutex_;                        // ������ ������������vector�������̰߳�ȫ����
    };

} // Vita

#endif //VITANETLIB_EVENTLOOP_H
