//
// Created by vitanmc on 2023/3/5.
//

#include "EventLoopThread.h"
#include "EventLoop.h"

namespace Vita {


    EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                     const std::string &name)
            : loop_(nullptr), exiting_(false), thread_(std::bind(&EventLoopThread::threadFunc, this), name), mutex_(),
              cond_(), callback_(cb) {
    }

    EventLoopThread::~EventLoopThread() {
        exiting_ = true;
        if (loop_ != nullptr) {
            loop_->quit();
            thread_.join();
        }
    }


    // EventLoopThread startLoop
    // 1. 开启一个线程，该线程的工作函数为下方的 EventLoopThread::threadFunc
    //      1.1 创建一个EventLoop对象！！！
    //      1.2 执行默认回调，该回调为EventLoopThread构造时传入
    //      1.3 给EventLoopThread的loop_指针赋上该EventLoop实例
    //      1.4 开启新Loop的事件循环
    // 2. 拿到1获得的新 EventLoop
    // 达到线程和EventLoop的绑定
    EventLoop *EventLoopThread::startLoop() {
        thread_.start(); // 启用底层线程Thread类对象thread_中通过start()创建的线程

        EventLoop *loop = nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (loop_ == nullptr) {
                cond_.wait(lock);
            }
            loop = loop_;
        }
        return loop;
    }

// 下面这个方法 是在单独的新线程里运行的
    void EventLoopThread::threadFunc() {
        // 创建一个独立的EventLoop对象 和上面的线程是一一对应的 级one loop per thread
        // 这个EventLoop是子Reactor的EventLoop
        // 与MainReactor完全不同

        EventLoop loop;

        if (callback_) {
            callback_(&loop);
        }

        {
            std::unique_lock<std::mutex> lock(mutex_);
            loop_ = &loop;
            cond_.notify_one();
        }
        // 执行EventLoop的loop() 开启了底层的Poller的poll()
        loop.loop();
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }


} // Vita