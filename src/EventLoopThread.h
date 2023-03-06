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


        //�ֳ���ʼ�����õĻص�������Ψһ����Ϊ EventLoop *
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                        const std::string &name = std::string());

        ~EventLoopThread();

        EventLoop *startLoop();

    private:
        void threadFunc();


        // EventLoopThread�������ǰ�һ��EventLoop��һ��Thread
        EventLoop *loop_;
        Thread thread_;
        bool exiting_;
        std::mutex mutex_;             // ������
        std::condition_variable cond_; // ��������
        ThreadInitCallback callback_;
    };

} // Vita

#endif //VITANETLIB_EVENTLOOPTHREAD_H
