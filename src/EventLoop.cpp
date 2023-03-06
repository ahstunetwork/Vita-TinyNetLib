//
// Created by vitanmc on 2023/3/5.
//

#include "EventLoop.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

#include "EventLoop.h"
#include "Logger.h"
#include "Channel.h"
#include "Poller.h"

namespace Vita {


// ��ֹһ���̴߳������EventLoop
    __thread EventLoop *t_loopInThisThread = nullptr;

// ����Ĭ�ϵ�Poller IO���ýӿڵĳ�ʱʱ��
    const int kPollTimeMs = 10000; // 10000���� = 10����

/* �����߳�֮�����̺߳����߳�˭�������ǲ�ȷ���ġ�
 * ͨ��һ��eventfd���߳�֮�䴫�����ݵĺô��Ƕ���߳����������Ϳ���ʵ��ͬ����
 * eventfd֧�ֵ�����ں˰汾ΪLinux 2.6.27,��2.6.26��֮ǰ�İ汾Ҳ����ʹ��eventfd������flags��������Ϊ0��
 * ����ԭ�ͣ�
 *     #include <sys/eventfd.h>
 *     int eventfd(unsigned int initval, int flags);
 * ����˵����
 *      initval,��ʼ����������ֵ��
 *      flags, EFD_NONBLOCK,����socketΪ��������
 *             EFD_CLOEXEC��ִ��fork��ʱ���ڸ������е����������Զ��رգ��ӽ����е�������������
 * ������
 *     eventfd��������ͬһ������֮�е��߳�֮���ͨ�š�
 *     eventfd����������ͬ��Ե��ϵ�Ľ���֮���ͨ�š�
 *     eventfd���ڲ�ͬ��Ե��ϵ�Ľ���֮��ͨ�ŵĻ���Ҫ��eventfd���ڼ������̹���Ĺ����ڴ��У�û�в��Թ�����
 */


// ����wakeupfd ����notify����subReactor����������channel
    int createEventfd() {
        int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evtfd < 0) {
            LOG_FATAL("eventfd error:%d\n", errno);
        }
        return evtfd;
    }


    // EventLoop�Ĺ��캯��
    // 1. ����һ���µ�ר��������Poller
    // 2. ����һ���µ�ר�������� wakeupfd��wakeupChannel
    //      2.1 ��һ��fd��ʼ�����̸�����װ��һ�� channel
    //      2.2 �����Ÿ�������һ��readCallback & enable����ʼ��
    EventLoop::EventLoop()
            : looping_(false), quit_(false), callingPendingFunctors_(false), threadId_(CurrentThread::tid()),
              poller_(Poller::newDefaultPoller(this)), wakeupFd_(createEventfd()),
              wakeupChannel_(new Channel(this, wakeupFd_)) {
        LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
        if (t_loopInThisThread) {
            LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
        } else {
            t_loopInThisThread = this;
        }

        // ����wakeupfd���¼������Լ������¼���Ļص�����
        // ���������򹤵ĵط�
        wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
        wakeupChannel_->enableReading(); // ÿһ��EventLoop��������wakeupChannel_��EPOLL���¼���
    }

    EventLoop::~EventLoop() {
        wakeupChannel_->disableAll(); // ��Channel�Ƴ����и���Ȥ���¼�
        wakeupChannel_->remove();     // ��Channel��EventLoop��ɾ����
        ::close(wakeupFd_);
        t_loopInThisThread = nullptr;
    }

// �����¼�ѭ��
// ѭ����������
// 1. ����eventloop���е�poller��epoll_wait���õ�������activeChannels
// 2. ����activeChannels��ѭ��ȡ��ÿ��channel���������Ǹ��Ե�handleEvent
// 3. ����߲�ص�PendingFunctors()
/**
 * ִ�е�ǰEventLoop�¼�ѭ����Ҫ����Ļص�����
 * �����߳��� >= 2 ����� IO�߳� mainloop(mainReactor) ��Ҫ������
 *      1. accept��������
 *      2. ��accept���ص�connfd���ΪChannel
 *      3. TcpServer::newConnectionͨ����ѯ��TcpConnection��������subloop����
 *
 * mainloop����queueInLoop���ص�����subloop
 * �ûص���Ҫsubloopִ�� ��subloop����poller_->poll������
 * queueInLoopͨ��wakeup��subloop����
 **/

    void EventLoop::loop() {
        looping_ = true;
        quit_ = false;

        LOG_INFO("EventLoop %p start looping\n", this);

        while (!quit_) {
            activeChannels_.clear();
            pollRetureTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
            for (Channel *channel: activeChannels_) {
                // Poller������Щchannel�������¼� Ȼ���ϱ���EventLoop ֪ͨchannel������Ӧ���¼�
                channel->handleEvent(pollRetureTime_);
            }
            doPendingFunctors();
        }
        LOG_INFO("EventLoop %p stop looping.\n", this);
        looping_ = false;
    }

/**
 * �˳��¼�ѭ��
 * 1. ���loop���Լ����߳��е���quit�ɹ��� ˵����ǰ�߳��Ѿ�ִ�������loop()������poller_->poll���˳�
 * 2. ������ǵ�ǰEventLoop�����߳��е���quit�˳�EventLoop ��Ҫ����EventLoop�����̵߳�epoll_wait
 *
 * ������һ��subloop(worker)�е���mainloop(IO)��quitʱ
 *  ��Ҫ����mainloop(IO)��poller_->poll ����ִ����loop()����
 *
 * ������ ע�⣺ ��������� mainloop������������ ���ص�д��subloop�� ͨ��������������ģ�ͼ���ʵ���̰߳�ȫ�Ķ���
 * ������       ����muduoͨ��wakeup()���� ʹ��eventfd������wakeupFd_ notify
 *             ʹ��mainloop��subloop֮���ܹ�����ͨ��
 **/
    void EventLoop::quit() {
        quit_ = true;

        if (!isInLoopThread()) {
            wakeup();
        }
    }

// �ڵ�ǰloop��ִ��cb
    void EventLoop::runInLoop(Functor cb) {
        if (isInLoopThread()) // ��ǰEventLoop��ִ�лص�
        {
            cb();
        } else // �ڷǵ�ǰEventLoop�߳���ִ��cb������Ҫ����EventLoop�����߳�ִ��cb
        {
            queueInLoop(cb);
        }
    }

// ��cb��������� ����loop���ڵ��߳�ִ��cb
/**
 * || callingPendingFunctors����˼�� ��ǰloop����ִ�лص���
 * ����loop��pendingFunctors_���ּ������µĻص� ��Ҫͨ��wakeupд�¼�
 * ������Ӧ����Ҫִ������ص�������loop���߳�
 * ��loop()��һ��poller_->poll()���������������Ļ����ӳ�ǰһ���¼���Ļص���ִ��
 * Ȼ�󣬼���ִ��pendingFunctors_�еĻص�����
 **/
    void EventLoop::queueInLoop(Functor cb) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            pendingFunctors_.emplace_back(cb);
        }

        //����Ǹ߲㷢�����Ļص������ҵ���loop����ִ�лص�
        //������wakeupһ�Σ�����������һ�ֵ�epoll_wait���Ӷ�ִ�д��ַ������ĸ߲�ص�
        if (!isInLoopThread() || callingPendingFunctors_) {
            wakeup(); // ����loop�����߳�
        }
    }




    // �ú�����ΪwakeupChannel��readCallback
    // �����Ƿ�Ҫ����Ϊread�汾�ģ�ֻҪ�ӵ�epfd����
    void EventLoop::handleRead() {
        uint64_t one = 1;
        ssize_t n = read(wakeupFd_, &one, sizeof(one));
        if (n != sizeof(one)) {
            LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
        }
    }

// ��������loop�����߳� ��wakeupFd_дһ������ wakeupChannel�ͷ������¼� ��ǰloop�߳̾ͻᱻ����
    void EventLoop::wakeup() {
        uint64_t one = 1;
        ssize_t n = write(wakeupFd_, &one, sizeof(one));
        if (n != sizeof(one)) {
            LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
        }
    }

// EventLoop�ķ��� => Poller�ķ���
    void EventLoop::updateChannel(Channel *channel) {
        poller_->updateChannel(channel);
    }

    void EventLoop::removeChannel(Channel *channel) {
        poller_->removeChannel(channel);
    }

    bool EventLoop::hasChannel(Channel *channel) {
        return poller_->hasChannel(channel);
    }




    //��eventloop������epoll_wait���ȵ���activeChannel����handleEvent
    //Ȼ��ᴦ��߲㷢���Ļص�����
    //����һ��������߲㷢�����ص�����������epoll_waitһ��û�з��أ�eventloopһֱ����
    //��ô�߲�ĺ�����Զû��ִ�еĿ��ܣ�����������wakeupfd���������epoll��
    //�߲�ص�������������ʱ�򣬰������ǵ��ڹ�wakeupfd)���epoll_wait������
    void EventLoop::doPendingFunctors() {
        std::vector<Functor> functors;
        callingPendingFunctors_ = true;    //��ʶ��ǰeventloop����ִ�и߲�ص�

        // �����ķ�ʽ�����������ٽ�����Χ ����Ч�� ͬʱ����������
        // ���ִ��functor()���ٽ����� ��functor()�е���queueInLoop()�ͻ��������
        {
            std::unique_lock<std::mutex> lock(mutex_);
            functors.swap(pendingFunctors_);
        }

        for (const Functor &functor: functors) {
            functor(); // ִ�е�ǰloop��Ҫִ�еĻص�����
        }
        callingPendingFunctors_ = false;
    }

} // Vita