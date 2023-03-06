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
    // 1. ����һ���̣߳����̵߳Ĺ�������Ϊ�·��� EventLoopThread::threadFunc
    //      1.1 ����һ��EventLoop���󣡣���
    //      1.2 ִ��Ĭ�ϻص����ûص�ΪEventLoopThread����ʱ����
    //      1.3 ��EventLoopThread��loop_ָ�븳�ϸ�EventLoopʵ��
    //      1.4 ������Loop���¼�ѭ��
    // 2. �õ�1��õ��� EventLoop
    // �ﵽ�̺߳�EventLoop�İ�
    EventLoop *EventLoopThread::startLoop() {
        thread_.start(); // ���õײ��߳�Thread�����thread_��ͨ��start()�������߳�

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

// ����������� ���ڵ��������߳������е�
    void EventLoopThread::threadFunc() {
        // ����һ��������EventLoop���� ��������߳���һһ��Ӧ�� ��one loop per thread
        // ���EventLoop����Reactor��EventLoop
        // ��MainReactor��ȫ��ͬ

        EventLoop loop;

        if (callback_) {
            callback_(&loop);
        }

        {
            std::unique_lock<std::mutex> lock(mutex_);
            loop_ = &loop;
            cond_.notify_one();
        }
        // ִ��EventLoop��loop() �����˵ײ��Poller��poll()
        loop.loop();
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }


} // Vita