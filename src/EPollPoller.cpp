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

    const int kNew = -1;    // ĳ��channel��û�����Poller          // channel�ĳ�Աstatus_��ʼ��Ϊ-1
    const int kAdded = 1;   // ĳ��channel�Ѿ������Poller
    const int kDeleted = 2; // ĳ��channel�Ѿ���Pollerɾ��



    // EPollPoller�Ĺ��캯������ʼ����������
    // 1. poller������ eventloopָ��
    // 2. epoll_create����ֵ����
    // 3. epoll_wait��Ҫ�����������Ҫresize
    EPollPoller::EPollPoller(EventLoop *loop)
            : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
              events_(kInitEventListSize) // vector<epoll_event>(16)
    {
        if (epollfd_ < 0) {     //���epoll_createʧ�ܣ����ص�����<0
            LOG_FATAL("epoll_create error:%d \n", errno);
        }
    }

    //�����������ر���������
    EPollPoller::~EPollPoller() {
        ::close(epollfd_);
    }


    //poll����������ʱ���������ؾ������ļ�����������activeChannels
    Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
        // ����Ƶ������poll ʵ����Ӧ����LOG_DEBUG�����־��Ϊ���� �������������� �ر�DEBUG��־����Ч��
        LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

        // ����epoll_wait�������У�
        // 1. epfd����
        // 2. ���վ����ļ����������ϵ������׵�ַ
        // 3. �������
        // 4. ��ʱʱ�䣬-1Ϊ��������
        int numEvents = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
//        int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
        int saveErrno = errno;
        Timestamp now(Timestamp::now());

        //����������ļ�����������>0��˵������л��������ǵ���fillActiveChannels
        if (numEvents > 0) {
            LOG_INFO("%d events happend\n", numEvents); // LOG_DEBUG�����

            // fillActiveChannels������
            // 1. �������ʼ�����������
            // 2. ���ڽ��յ�����
            fillActiveChannels(numEvents, activeChannels);
            //����������ļ������������ﵽ�����������ֵ�������ݶ�����������ֱ��resize
            if (numEvents == events_.size()) {
                events_.resize(events_.size() * 2);
            }
            // �����ļ�����������Ϊ0�����ܷ�������������epoll_wait�����ܷ���
        } else if (numEvents == 0) {
            LOG_DEBUG("%s timeout!\n", __FUNCTION__);
        } else {
            if (saveErrno != EINTR) {
                errno = saveErrno;
                LOG_ERROR("EPollPoller::poll() error!");
            }
        }
        //����ʱ������ڵ���
        return now;
    }

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
// updateChannel�����������ǲ����õ��յ�վ����Ϊ����epollpoller�����Ǳ��봦���
// updateChannel�����ı��⣺��Ҫ��ע��fd�ӵ�poll���ϣ�ʹfd��Ϊ�����ˣ�Ϊʲô���ڽ���cfd��ʱ��
// �ͽ�����ص�epfd����?��Ϊ��Ƶ�ԭ�򣬽�����������Acceptor�������ģ�Acceptor����ֱ�Ӻ�poll�򽻵�
// ��ͬ���еľ��Ƿ�װ��fd�ľ�̬channel������updatechannel����վ����channel������eventloop��ת
void EPollPoller::updateChannel(Channel *channel) {
        const int status = channel->get_status();
        LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->get_fd(), channel->get_events(), status);

        //����channel�Ѿ����õ�״̬��Ĭ�Ϲ���ʱ���״̬��-1
        if (status == kNew || status == kDeleted) {
            //������½��ģ���û�м���poll���Ǿͼ���
            if (status == kNew) {
                int fd = channel->get_fd();
                //�����poller��mapС������
                channels_[fd] = channel;
            } else // status == kAdd
            { }
            // Ϊchannel����״̬
            channel->set_status(kAdded);
            //����updata������update����epoll_ctl��������channel.fdʵ�ʼ�������
            update(EPOLL_CTL_ADD, channel);
        } else // channel�Ѿ���Poller��ע�����
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

// ��Poller��ɾ��channel
    void EPollPoller::removeChannel(Channel *channel) {
        int fd = channel->get_fd();
        //����poll������ɾ��
        channels_.erase(fd);

        LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

        int status = channel->get_status();
        //����Ѿ������poll�У���ô��ɾ��
        if (status == kAdded) {
            update(EPOLL_CTL_DEL, channel);
        }
        channel->set_status(kNew);
    }

// ǧ����ʼ��������д��Ծ������
// epoll_wait���صľ�����fds��ֱ�ӷ��ز��ͺ��ˣ�ΪʲôҪ��תһ�֣�
// ����������ǣ�std::vector<epoll_event>������channel������data.ptr���桾reactor��
// ˼����data.ptr -> channel ��˭����ȥ���أ�
// �������Լ�����updateChannel����fd��Ϊ���������ʱ�򣬵��õ�update������epoll_ctl
// �ҵ����ϵ�ʱ�� epoll_ctl����Ҫepoll_event������ʱ����ֻ��Ҫ������channel
    void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
        for (int i = 0; i < numEvents; ++i) {
            Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
            channel->set_revents(events_[i].events);
            activeChannels->push_back(channel); // EventLoop���õ�������Poller�������ص����з����¼���channel�б���
        }
    }

// ����channelͨ�� ��ʵ���ǵ���epoll_ctl add/mod/del
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