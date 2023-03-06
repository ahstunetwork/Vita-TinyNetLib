//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_THREAD_H
#define VITANETLIB_THREAD_H

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

#include "noncopyable.h"

namespace Vita {


    class Thread : NonCopyable {
    public:

        //�̺߳���
        using ThreadFunc = std::function<void()>;
        explicit Thread(ThreadFunc, const std::string &name = std::string());

        ~Thread();

        void start();
        void join();
        bool started() { return started_; }

        pid_t tid() const { return tid_; }
        const std::string &name() const { return name_; }
        static int numCreated() { return numCreated_; }

    private:
        void setDefaultName();
        bool started_;
        bool joined_;

        //����һ����׼���̵߳�ָ��
        std::shared_ptr<std::thread> thread_;
        pid_t tid_;       // ���̴߳���ʱ�ٰ�
        ThreadFunc func_; // �̻߳ص�����
        std::string name_;// �߳�����
        static std::atomic_int numCreated_;   //Ϊ���̵߳�Ĭ�����֣�Thread 1
    };

} // Vita

#endif //VITANETLIB_THREAD_H
