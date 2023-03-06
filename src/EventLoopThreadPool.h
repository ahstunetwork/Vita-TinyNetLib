//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_EVENTLOOPTHREADPOOL_H
#define VITANETLIB_EVENTLOOPTHREADPOOL_H

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "noncopyable.h"

namespace Vita {

    class EventLoop;

    class EventLoopThread;

    class EventLoopThreadPool : NonCopyable {
    public:
        //线程初始化回调，在EventLoopThread同样也有他
        using ThreadInitCallback = std::function<void(EventLoop *)>;


        //构造函数需要一个baseLoop
        EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);

        ~EventLoopThreadPool();

        void setThreadNum(int numThreads) { numThreads_ = numThreads; }

        void start(const ThreadInitCallback &cb = ThreadInitCallback());

        // 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
        EventLoop *getNextLoop();

        // 以vector<EventLoop*>的形式返回素有的loop
        std::vector<EventLoop *> getAllLoops();

        bool started() const { return started_; }

        const std::string name() const { return name_; }

    private:
        // 用户使用muduo创建的loop 如果线程数为1 那直接使用用户创建的loop 否则创建多EventLoop
        EventLoop *baseLoop_;
        std::string name_;
        bool started_;
        int numThreads_;
        int next_; // 轮询的下标
        //他身为管理EventLoopThread的类，持有所有的EventLoopThtread指针
        std::vector<std::unique_ptr<EventLoopThread>> threads_;
        std::vector<EventLoop *> loops_;
    };

} // Vita

#endif //VITANETLIB_EVENTLOOPTHREADPOOL_H
