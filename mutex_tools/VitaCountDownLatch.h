//
// Created by vitanmc on 2023/2/24.
//

#ifndef MYNETLIB_VITACOUNTDOWNLATCH_H
#define MYNETLIB_VITACOUNTDOWNLATCH_H

#include "VitaMutex.h"
#include "VitaCondition.h"

namespace Vita {

    class VitaCountDownLatch {
    public:
        explicit VitaCountDownLatch(int count)
                : count(count), vitaMutex(), vitaCondition(vitaMutex) {}

        void wait() {
            VitaLockGuard vitaLockGuard(vitaMutex);
            while (count > 0) {
                vitaCondition.wait();
            }
        }

        void countDown() {
            VitaLockGuard vitaLockGuard(vitaMutex);
            count--;
            if (count <= 0) {
                vitaCondition.notifyAll();
            }
        }

        int getCount() const {
            VitaLockGuard lockGuard(vitaMutex);
            return count;
        }

    private:
        int count;
        mutable VitaMutex vitaMutex;
        VitaCondition vitaCondition;
    };

} // Vita

#endif //MYNETLIB_VITACOUNTDOWNLATCH_H
