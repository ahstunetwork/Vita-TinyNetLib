//
// Created by vitanmc on 2023/3/5.
//

#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

namespace Vita {

    // 原子整形，为了线程名字
    std::atomic_int Thread::numCreated_(0);

    Thread::Thread(ThreadFunc func, const std::string &name)
            : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name) {
        setDefaultName();
    }

    Thread::~Thread() {
        if (started_ && !joined_) {
            thread_->detach();                                                  // thread类提供了设置分离线程的方法 线程运行后自动销毁（非阻塞）
        }
    }


    //线程类构造的时候没有构造std::Thread
    //真正构造的地方是在start里面
    void Thread::start()                                                        // 一个Thread对象 记录的就是一个新线程的详细信息
    {
        started_ = true;
        sem_t sem;
        sem_init(&sem, false, 0);                                               // false指的是 不设置进程间共享
        // 开启线程
        // 用lambda构造一个线程对象，让其运行Vita::Thread传入的函数 func_
        // 为何不直接用 func_ 初始化线程对象？为了让其多运行一点代码
        thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
            tid_ = CurrentThread::tid();
            sem_post(&sem);
            func_();
        }));

        // 这里必须等待获取上面新创建的线程的tid值
        // sem_wait会在这里阻塞，上面调用sem_post会解除阻塞，可以保证线程创建完整
        // 使用 countDownLatch 也行
        sem_wait(&sem);
    }

// C++ std::thread 中join()和detach()的区别：https://blog.nowcoder.net/n/8fcd9bb6e2e94d9596cf0a45c8e5858a
    void Thread::join() {
        joined_ = true;
        thread_->join();
    }

    void Thread::setDefaultName() {
        int num = ++numCreated_;
        if (name_.empty()) {
            char buf[32] = {0};
            snprintf(buf, sizeof buf, "Thread%d", num);
            name_ = buf;
        }
    }


} // Vita