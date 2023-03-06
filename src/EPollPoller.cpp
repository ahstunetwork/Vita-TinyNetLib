//
// Created by vitanmc on 2023/3/5.
//

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

namespace Vita {

    const int kNew = -1;    // 某个channel还没添加至Poller          // channel的成员status_初始化为-1
    const int kAdded = 1;   // 某个channel已经添加至Poller
    const int kDeleted = 2; // 某个channel已经从Poller删除



    // EPollPoller的构造函数，初始化的数据有
    // 1. poller所处的 eventloop指针
    // 2. epoll_create返回值树根
    // 3. epoll_wait需要传入的数组需要resize
    EPollPoller::EPollPoller(EventLoop *loop)
            : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
              events_(kInitEventListSize) // vector<epoll_event>(16)
    {
        if (epollfd_ < 0) {     //如果epoll_create失败，返回的树根<0
            LOG_FATAL("epoll_create error:%d \n", errno);
        }
    }

    //析构函数，关闭树根即可
    EPollPoller::~EPollPoller() {
        ::close(epollfd_);
    }


    //poll函数，调用时阻塞，返回就绪的文件描述符集合activeChannels
    Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
        // 由于频繁调用poll 实际上应该用LOG_DEBUG输出日志更为合理 当遇到并发场景 关闭DEBUG日志提升效率
        LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

        // 调用epoll_wait，参数有：
        // 1. epfd树根
        // 2. 接收就绪文件描述符集合的数组首地址
        // 3. 最大数量
        // 4. 超时时间，-1为阻塞监听
        int numEvents = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
//        int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
        int saveErrno = errno;
        Timestamp now(Timestamp::now());

        //如果就绪的文件描述符集合>0，说明真的有货，那我们调用fillActiveChannels
        if (numEvents > 0) {
            LOG_INFO("%d events happend\n", numEvents); // LOG_DEBUG最合理

            // fillActiveChannels参数：
            // 1. 就绪的问价描述符个数
            // 2. 用于接收的数组
            fillActiveChannels(numEvents, activeChannels);
            //如果就绪的文件描述符个数达到数组容量最大值，就扩容二倍，无他，直接resize
            if (numEvents == events_.size()) {
                events_.resize(events_.size() * 2);
            }
            // 就绪文件描述符数量为0？可能发生吗？阻塞调用epoll_wait不可能发生
        } else if (numEvents == 0) {
            LOG_DEBUG("%s timeout!\n", __FUNCTION__);
        } else {
            if (saveErrno != EINTR) {
                errno = saveErrno;
                LOG_ERROR("EPollPoller::poll() error!");
            }
        }
        //返回时间戳用于调试
        return now;
    }

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
// updateChannel操作，这里是层层调用的终点站，因为到了epollpoller这里是必须处理的
// updateChannel函数的本意：将要关注的fd加到poll树上，使fd成为正常人，为什么不在接收cfd的时候
// 就将其挂载到epfd树上?因为设计的原因，接收新连接是Acceptor类来做的，Acceptor并不直接和poll打交道
// 共同持有的就是封装了fd的静态channel，并且updatechannel的首站就是channel，借助eventloop中转
void EPollPoller::updateChannel(Channel *channel) {
        const int status = channel->get_status();
        LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->get_fd(), channel->get_events(), status);

        //检验channel已经设置的状态，默认构造时候的状态是-1
        if (status == kNew || status == kDeleted) {
            //如果是新建的，还没有加入poll，那就加入
            if (status == kNew) {
                int fd = channel->get_fd();
                //存入该poller的map小本本里
                channels_[fd] = channel;
            } else // status == kAdd
            { }
            // 为channel更新状态
            channel->set_status(kAdded);
            //调用updata操作，update调用epoll_ctl函数，将channel.fd实际加入树中
            update(EPOLL_CTL_ADD, channel);
        } else // channel已经在Poller中注册过了
        {
            int fd = channel->get_fd();
            if (channel->isNoneEvent()) {
                update(EPOLL_CTL_DEL, channel);
                channel->set_status(kDeleted);
            } else {
                update(EPOLL_CTL_MOD, channel);
            }
        }
    }

// 从Poller中删除channel
    void EPollPoller::removeChannel(Channel *channel) {
        int fd = channel->get_fd();
        //现在poll备案里删除
        channels_.erase(fd);

        LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

        int status = channel->get_status();
        //如果已经添加在poll中，那么就删除
        if (status == kAdded) {
            update(EPOLL_CTL_DEL, channel);
        }
        channel->set_status(kNew);
    }

// 千呼万唤始出来：填写活跃的连接
// epoll_wait返回的就绪的fds，直接返回不就好了，为什么要中转一手？
// 数组的数据是：std::vector<epoll_event>，真正channel数据在data.ptr里面【reactor】
// 思考：data.ptr -> channel 是谁挂上去的呢？
// 就是他自己！在updateChannel（将fd变为正规军）的时候，调用的update操作，epoll_ctl
// 挂到树上的时候 epoll_ctl才需要epoll_event，其他时候，他只需要是死的channel
    void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
        for (int i = 0; i < numEvents; ++i) {
            Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
            channel->set_revents(events_[i].events);
            activeChannels->push_back(channel); // EventLoop就拿到了它的Poller给它返回的所有发生事件的channel列表了
        }
    }

// 更新channel通道 其实就是调用epoll_ctl add/mod/del
    void EPollPoller::update(int operation, Channel *channel) {
        epoll_event event;
        ::memset(&event, 0, sizeof(event));

        int fd = channel->get_fd();

        event.events = channel->get_events();
        event.data.fd = fd;
        event.data.ptr = channel;

        if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
            if (operation == EPOLL_CTL_DEL) {
                LOG_ERROR("epoll_ctl del error:%d\n", errno);
            } else {
                LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
            }
        }
    }


} // Vita