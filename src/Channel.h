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
        using EventCallback = std::function<void()>;    //普通事件回调
        using ReadEventCallback = std::function<void(Timestamp)>; //读事件回调

        Channel(EventLoop *loop, int fd);

        ~Channel();

        // 当fd被epoll_wait触发以后，处理事件在handleEvent调用
        void handleEvent(Timestamp receiveTime);

        void handleEventWithGuard(Timestamp receiveTime);


        // 设置文件描述符对应事件的回调函数
        void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }

        void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }

        void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }

        void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }


        // 设置fd相应的事件状态 相当于epoll_ctl add delete
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

        // 返回fd当前的事件状态
        bool isNoneEvent() const { return events_ == kNoneEvent; }

        bool isWriting() const { return events_ & kWriteEvent; }

        bool isReading() const { return events_ & kReadEvent; }


        int get_status() { return status_; }

        void set_status(int idx) { status_ = idx; }



        // 防止当channel被手动remove掉 channel还在执行回调操作
        void tie(const std::shared_ptr<void> &);

        int get_fd() const { return fd_; }

        int get_events() const { return events_; }

        void set_revents(int revt) { revents_ = revt; }


        // one loop per thread
        EventLoop *ownerLoop() { return loop_; }

        void remove();

        void update();


    private:

        EventLoop *loop_;    //该channel所在的loop
        int fd_;   // 与channel绑定的 fd
        int events_;  // fd所关心的事件
        int revents_;  //epoll_wait触发实际发现fd的事件

        int status_;   //当前channel的状态


        // 一些设置与fd状态有关的常量
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
