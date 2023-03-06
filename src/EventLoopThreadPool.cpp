//
// Created by vitanmc on 2023/3/5.
//

#include "EventLoopThreadPool.h"
#include <memory>

#include "EventLoopThread.h"

namespace Vita {


    //���캯����ֻ�ǳ�ʼ������
    EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
            : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0) {
    }

    EventLoopThreadPool::~EventLoopThreadPool() {
        // Don't delete loop, it's stack variable
    }


    // ���ThreadInitCallBack���������Ǵ���EventLoopTHread��
    // ��ʼnumThreads_������EventLoopThread���߳�
    void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
        started_ = true;

        for (int i = 0; i < numThreads_; ++i) {
            char buf[name_.size() + 32];
            snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
            EventLoopThread *t = new EventLoopThread(cb, buf);
            // �ײ㴴���߳� ��һ���µ�EventLoop �����ظ�loop�ĵ�ַ
            threads_.push_back(std::unique_ptr<EventLoopThread>(t));
            //t->startLoop�Ĺ��ܣ�����һ���̣߳����ҷ���һ��pool
            loops_.push_back(t->startLoop());
        }

        // ���������ֻ��һ���߳�����baseLoop
        if (numThreads_ == 0 && cb) {
            cb(baseLoop_);
        }
    }

// ��������ڶ��߳��У�baseLoop_(mainLoop)��Ĭ������ѯ�ķ�ʽ����Channel��subLoop
    EventLoop *EventLoopThreadPool::getNextLoop() {
        EventLoop *loop = baseLoop_;    // ���ֻ����һ���߳� Ҳ����ֻ��һ��mainReactor ��subReactor ��ô��ѯֻ��һ���߳� getNextLoop()ÿ�ζ����ص�ǰ��baseLoop_

        if (!loops_.empty())             // ͨ����ѯ��ȡ��һ�������¼���loop
        {
            loop = loops_[next_];
            ++next_;
            if (next_ >= loops_.size()) {
                next_ = 0;
            }
        }

        return loop;
    }

    std::vector<EventLoop *> EventLoopThreadPool::getAllLoops() {
        if (loops_.empty()) {
            return std::vector<EventLoop *>(1, baseLoop_);
        } else {
            return loops_;
        }
    }


} // Vita