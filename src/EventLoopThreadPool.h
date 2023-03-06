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
        //�̳߳�ʼ���ص�����EventLoopThreadͬ��Ҳ����
        using ThreadInitCallback = std::function<void(EventLoop *)>;


        //���캯����Ҫһ��baseLoop
        EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);

        ~EventLoopThreadPool();

        void setThreadNum(int numThreads) { numThreads_ = numThreads; }

        void start(const ThreadInitCallback &cb = ThreadInitCallback());

        // ��������ڶ��߳��У�baseLoop_(mainLoop)��Ĭ������ѯ�ķ�ʽ����Channel��subLoop
        EventLoop *getNextLoop();

        // ��vector<EventLoop*>����ʽ�������е�loop
        std::vector<EventLoop *> getAllLoops();

        bool started() const { return started_; }

        const std::string name() const { return name_; }

    private:
        // �û�ʹ��muduo������loop ����߳���Ϊ1 ��ֱ��ʹ���û�������loop ���򴴽���EventLoop
        EventLoop *baseLoop_;
        std::string name_;
        bool started_;
        int numThreads_;
        int next_; // ��ѯ���±�
        //����Ϊ����EventLoopThread���࣬�������е�EventLoopThtreadָ��
        std::vector<std::unique_ptr<EventLoopThread>> threads_;
        std::vector<EventLoop *> loops_;
    };

} // Vita

#endif //VITANETLIB_EVENTLOOPTHREADPOOL_H
