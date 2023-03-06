//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_EVENTLOOPTHREAD_H
#define VITANETLIB_EVENTLOOPTHREAD_H

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

#include "noncopyable.h"
#include "Thread.h"

namespace Vita {


    class EventLoop;

    class EventLoopThread : NonCopyable {
    public:


        //现场初始化调用的回调函数，唯一参数为 EventLoop *
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                        const std::string &name = std::string());

        ~EventLoopThread();

        EventLoop *startLoop();

    private:
        void threadFunc();


        // EventLoopThread的作用是绑定一个EventLoop和一个Thread
        EventLoop *loop_;
        Thread thread_;
        bool exiting_;
        std::mutex mutex_;             // 互斥锁
        std::condition_variable cond_; // 条件变量
        ThreadInitCallback callback_;
    };

} // Vita

#endif //VITANETLIB_EVENTLOOPTHREAD_H
