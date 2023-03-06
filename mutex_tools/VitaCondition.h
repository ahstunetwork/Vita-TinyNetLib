//
// Created by vitanmc on 2023/2/24.
//

#ifndef MYNETLIB_VITACONDITION_H
#define MYNETLIB_VITACONDITION_H

#include "VitaMutex.h"
#include <pthread.h>

namespace Vita {

    class VitaCondition {
    public:
        explicit VitaCondition(VitaMutex &vitaMutex) :
                vitaMutex(vitaMutex), pthreadCond() {
            pthread_cond_init(&pthreadCond, NULL);
        }

        void wait() {
            pthread_cond_wait(&pthreadCond, &vitaMutex.get_raw_mutex());
        }

        void notify() {
            pthread_cond_signal(&pthreadCond);
        }

        void notifyAll() {
            pthread_cond_broadcast(&pthreadCond);
        }

        ~VitaCondition() {
            pthread_cond_destroy(&pthreadCond);
        }

    private:
        // hold the same mutex as raw-mutex
        VitaMutex &vitaMutex;
        pthread_cond_t pthreadCond;
    };

} // Vita

#endif //MYNETLIB_VITACONDITION_H
