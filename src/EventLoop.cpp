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


// 防止一个线程创建多个EventLoop
    __thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
    const int kPollTimeMs = 10000; // 10000毫秒 = 10秒钟

/* 创建线程之后主线程和子线程谁先运行是不确定的。
 * 通过一个eventfd在线程之间传递数据的好处是多个线程无需上锁就可以实现同步。
 * eventfd支持的最低内核版本为Linux 2.6.27,在2.6.26及之前的版本也可以使用eventfd，但是flags必须设置为0。
 * 函数原型：
 *     #include <sys/eventfd.h>
 *     int eventfd(unsigned int initval, int flags);
 * 参数说明：
 *      initval,初始化计数器的值。
 *      flags, EFD_NONBLOCK,设置socket为非阻塞。
 *             EFD_CLOEXEC，执行fork的时候，在父进程中的描述符会自动关闭，子进程中的描述符保留。
 * 场景：
 *     eventfd可以用于同一个进程之中的线程之间的通信。
 *     eventfd还可以用于同亲缘关系的进程之间的通信。
 *     eventfd用于不同亲缘关系的进程之间通信的话需要把eventfd放在几个进程共享的共享内存中（没有测试过）。
 */


// 创建wakeupfd 用来notify唤醒subReactor处理新来的channel
    int createEventfd() {
        int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evtfd < 0) {
            LOG_FATAL("eventfd error:%d\n", errno);
        }
        return evtfd;
    }


    // EventLoop的构造函数
    // 1. 构造一个新的专属于他的Poller
    // 2. 构造一个新的专属于他的 wakeupfd和wakeupChannel
    //      2.1 从一个fd开始，立刻给他武装成一个 channel
    //      2.2 紧接着给他设置一个readCallback & enable，开始打工
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

        // 设置wakeupfd的事件类型以及发生事件后的回调操作
        // 等下有他打工的地方
        wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
        wakeupChannel_->enableReading(); // 每一个EventLoop都将监听wakeupChannel_的EPOLL读事件了
    }

    EventLoop::~EventLoop() {
        wakeupChannel_->disableAll(); // 给Channel移除所有感兴趣的事件
        wakeupChannel_->remove();     // 把Channel从EventLoop上删除掉
        ::close(wakeupFd_);
        t_loopInThisThread = nullptr;
    }

// 开启事件循环
// 循环做三件事
// 1. 调用eventloop持有的poller的epoll_wait，得到就绪的activeChannels
// 2. 对于activeChannels，循环取出每个channel，调用他们各自的handleEvent
// 3. 处理高层回调PendingFunctors()
/**
 * 执行当前EventLoop事件循环需要处理的回调操作
 * 对于线程数 >= 2 的情况 IO线程 mainloop(mainReactor) 主要工作：
 *      1. accept接收连接
 *      2. 将accept返回的connfd打包为Channel
 *      3. TcpServer::newConnection通过轮询将TcpConnection对象分配给subloop处理
 *
 * mainloop调用queueInLoop将回调加入subloop
 * 该回调需要subloop执行 但subloop还在poller_->poll处阻塞
 * queueInLoop通过wakeup将subloop唤醒
 **/

    void EventLoop::loop() {
        looping_ = true;
        quit_ = false;

        LOG_INFO("EventLoop %p start looping\n", this);

        while (!quit_) {
            activeChannels_.clear();
            pollRetureTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
            for (Channel *channel: activeChannels_) {
                // Poller监听哪些channel发生了事件 然后上报给EventLoop 通知channel处理相应的事件
                channel->handleEvent(pollRetureTime_);
            }
            doPendingFunctors();
        }
        LOG_INFO("EventLoop %p stop looping.\n", this);
        looping_ = false;
    }

/**
 * 退出事件循环
 * 1. 如果loop在自己的线程中调用quit成功了 说明当前线程已经执行完毕了loop()函数的poller_->poll并退出
 * 2. 如果不是当前EventLoop所属线程中调用quit退出EventLoop 需要唤醒EventLoop所属线程的epoll_wait
 *
 * 比如在一个subloop(worker)中调用mainloop(IO)的quit时
 *  需要唤醒mainloop(IO)的poller_->poll 让其执行完loop()函数
 *
 * ！！！ 注意： 正常情况下 mainloop负责请求连接 将回调写入subloop中 通过生产者消费者模型即可实现线程安全的队列
 * ！！！       但是muduo通过wakeup()机制 使用eventfd创建的wakeupFd_ notify
 *             使得mainloop和subloop之间能够进行通信
 **/
    void EventLoop::quit() {
        quit_ = true;

        if (!isInLoopThread()) {
            wakeup();
        }
    }

// 在当前loop中执行cb
    void EventLoop::runInLoop(Functor cb) {
        if (isInLoopThread()) // 当前EventLoop中执行回调
        {
            cb();
        } else // 在非当前EventLoop线程中执行cb，就需要唤醒EventLoop所在线程执行cb
        {
            queueInLoop(cb);
        }
    }

// 把cb放入队列中 唤醒loop所在的线程执行cb
/**
 * || callingPendingFunctors的意思是 当前loop正在执行回调中
 * 但是loop的pendingFunctors_中又加入了新的回调 需要通过wakeup写事件
 * 唤醒相应的需要执行上面回调操作的loop的线程
 * 让loop()下一次poller_->poll()不再阻塞（阻塞的话会延迟前一次新加入的回调的执行
 * 然后，继续执行pendingFunctors_中的回调函数
 **/
    void EventLoop::queueInLoop(Functor cb) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            pendingFunctors_.emplace_back(cb);
        }

        //如果是高层发下来的回调，并且当先loop正在执行回调
        //则立即wakeup一次，立即唤醒新一轮的epoll_wait，从而执行此轮发下来的高层回调
        if (!isInLoopThread() || callingPendingFunctors_) {
            wakeup(); // 唤醒loop所在线程
        }
    }




    // 该函数作为wakeupChannel的readCallback
    // 倒不是非要设置为read版本的，只要加到epfd即可
    void EventLoop::handleRead() {
        uint64_t one = 1;
        ssize_t n = read(wakeupFd_, &one, sizeof(one));
        if (n != sizeof(one)) {
            LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
        }
    }

// 用来唤醒loop所在线程 向wakeupFd_写一个数据 wakeupChannel就发生读事件 当前loop线程就会被唤醒
    void EventLoop::wakeup() {
        uint64_t one = 1;
        ssize_t n = write(wakeupFd_, &one, sizeof(one));
        if (n != sizeof(one)) {
            LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
        }
    }

// EventLoop的方法 => Poller的方法
    void EventLoop::updateChannel(Channel *channel) {
        poller_->updateChannel(channel);
    }

    void EventLoop::removeChannel(Channel *channel) {
        poller_->removeChannel(channel);
    }

    bool EventLoop::hasChannel(Channel *channel) {
        return poller_->hasChannel(channel);
    }




    //在eventloop阻塞在epoll_wait后，先调用activeChannel对于handleEvent
    //然后会处理高层发来的回调函数
    //考虑一种情况，高层发下来回调函数，但是epoll_wait一种没有返回，eventloop一直阻塞
    //那么高层的函数永远没有执行的可能，所以设置了wakeupfd，将其加入epoll里
    //高层回调函数发下来的时候，安排我们的内鬼（wakeupfd)解除epoll_wait的阻塞
    void EventLoop::doPendingFunctors() {
        std::vector<Functor> functors;
        callingPendingFunctors_ = true;    //标识当前eventloop正在执行高层回调

        // 交换的方式减少了锁的临界区范围 提升效率 同时避免了死锁
        // 如果执行functor()在临界区内 且functor()中调用queueInLoop()就会产生死锁
        {
            std::unique_lock<std::mutex> lock(mutex_);
            functors.swap(pendingFunctors_);
        }

        for (const Functor &functor: functors) {
            functor(); // 执行当前loop需要执行的回调操作
        }
        callingPendingFunctors_ = false;
    }

} // Vita