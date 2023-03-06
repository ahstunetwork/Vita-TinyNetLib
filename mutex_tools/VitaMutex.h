//
// Created by vitanmc on 2023/2/23.
//

#ifndef MYNETLIB_VITAMUTEX_H
#define MYNETLIB_VITAMUTEX_H

#include <pthread.h>
#include <iostream>


namespace Vita {
    class VitaMutex {
    public:
        VitaMutex() {
            pthread_mutex_init(&mutex, nullptr);
        };

        // just for lockGuard
        void lock() {
            pthread_mutex_lock(&mutex);
        }

        // just for lockGuard
        void unlock() {
            pthread_mutex_unlock(&mutex);
        }

        // just for lockGuard
        pthread_mutex_t &get_raw_mutex() {
            return mutex;
        }


        ~VitaMutex() {
            pthread_mutex_destroy(&mutex);
        }

    private:
        pthread_mutex_t mutex{};
    };

} // Vita


namespace Vita { //Vita
    class VitaLockGuard {
    public:
        explicit VitaLockGuard(VitaMutex &vitaMutex) : vitaMutex(vitaMutex) {
            vitaMutex.lock();
//            std::cout << "lockGurad lock" << std::endl;
        }

        ~VitaLockGuard() {
//            std::cout << "lockGurad unlock" << std::endl;
            vitaMutex.unlock();
        }

    private:
        VitaMutex &vitaMutex;
        pthread_cond_t pthreadCond{};
    };

} //Vita



#endif //MYNETLIB_VITAMUTEX_H
