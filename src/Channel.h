//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_CHANNEL_H
#define VITANETLIB_CHANNEL_H

#include "NonCopyable.h"
#include "Timestamp.h"
#include <functional>
#include <memory>

namespace Vita {

    class EventLoop;

    class Channel : NonCopyable {
    public:
        using EventCallback = std::function<void()>;    //��ͨ�¼��ص�
        using ReadEventCallback = std::function<void(Timestamp)>; //���¼��ص�

        Channel(EventLoop *loop, int fd);

        ~Channel();

        // ��fd��epoll_wait�����Ժ󣬴����¼���handleEvent����
        void handleEvent(Timestamp receiveTime);

        void handleEventWithGuard(Timestamp receiveTime);


        // �����ļ���������Ӧ�¼��Ļص�����
        void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }

        void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }

        void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }

        void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }


        // ����fd��Ӧ���¼�״̬ �൱��epoll_ctl add delete
        void enableReading() {
            events_ |= kReadEvent;
            update();
        }

        void disableReading() {
            events_ &= ~kReadEvent;
            update();
        }

        void enableWriting() {
            events_ |= kWriteEvent;
            update();
        }

        void disableWriting() {
            events_ &= ~kWriteEvent;
            update();
        }

        void disableAll() {
            events_ = kNoneEvent;
            update();
        }

        // ����fd��ǰ���¼�״̬
        bool isNoneEvent() const { return events_ == kNoneEvent; }

        bool isWriting() const { return events_ & kWriteEvent; }

        bool isReading() const { return events_ & kReadEvent; }


        int get_status() { return status_; }

        void set_status(int idx) { status_ = idx; }



        // ��ֹ��channel���ֶ�remove�� channel����ִ�лص�����
        void tie(const std::shared_ptr<void> &);

        int get_fd() const { return fd_; }

        int get_events() const { return events_; }

        void set_revents(int revt) { revents_ = revt; }


        // one loop per thread
        EventLoop *ownerLoop() { return loop_; }

        void remove();

        void update();


    private:

        EventLoop *loop_;    //��channel���ڵ�loop
        int fd_;   // ��channel�󶨵� fd
        int events_;  // fd�����ĵ��¼�
        int revents_;  //epoll_wait����ʵ�ʷ���fd���¼�

        int status_;   //��ǰchannel��״̬


        // һЩ������fd״̬�йصĳ���
        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        ReadEventCallback readCallback_;
        EventCallback writeCallback_;
        EventCallback closeCallback_;
        EventCallback errorCallback_;

        std::weak_ptr<void> tie_;
        bool tied_;
    };

} // Vita

#endif //VITANETLIB_CHANNEL_H
