//
// Created by vitanmc on 2023/3/5.
//

#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

namespace Vita {

    // ԭ�����Σ�Ϊ���߳�����
    std::atomic_int Thread::numCreated_(0);

    Thread::Thread(ThreadFunc func, const std::string &name)
            : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name) {
        setDefaultName();
    }

    Thread::~Thread() {
        if (started_ && !joined_) {
            thread_->detach();                                                  // thread���ṩ�����÷����̵߳ķ��� �߳����к��Զ����٣���������
        }
    }


    //�߳��๹���ʱ��û�й���std::Thread
    //��������ĵط�����start����
    void Thread::start()                                                        // һ��Thread���� ��¼�ľ���һ�����̵߳���ϸ��Ϣ
    {
        started_ = true;
        sem_t sem;
        sem_init(&sem, false, 0);                                               // falseָ���� �����ý��̼乲��
        // �����߳�
        // ��lambda����һ���̶߳�����������Vita::Thread����ĺ��� func_
        // Ϊ�β�ֱ���� func_ ��ʼ���̶߳���Ϊ�����������һ�����
        thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
            tid_ = CurrentThread::tid();
            sem_post(&sem);
            func_();
        }));

        // �������ȴ���ȡ�����´������̵߳�tidֵ
        // sem_wait���������������������sem_post�������������Ա�֤�̴߳�������
        // ʹ�� countDownLatch Ҳ��
        sem_wait(&sem);
    }

// C++ std::thread ��join()��detach()������https://blog.nowcoder.net/n/8fcd9bb6e2e94d9596cf0a45c8e5858a
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