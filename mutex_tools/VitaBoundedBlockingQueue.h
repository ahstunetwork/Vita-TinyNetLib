//
// Created by vitanmc on 2023/2/24.
//

#ifndef MYNETLIB_VITABLOCKINGQUEUE_H
#define MYNETLIB_VITABLOCKINGQUEUE_H

#include "VitaMutex.h"
#include "VitaCondition.h"
#include <queue>

namespace Vita {

    template<class T>
    class VitaBoundedBlockingQueue {
    public:
        explicit VitaBoundedBlockingQueue(int capacity)
                : capacity(capacity), mutex(),
                  condNotEmpty(mutex),
                  condNotFull(mutex) {}

        void put(T val) {
            VitaLockGuard lockGuard(mutex);
            while (productsQueue.size() >= capacity) {
                std::cout << "full put wait !" << std::endl;
                condNotFull.wait();
            }
            productsQueue.push(val);
            std::cout << "push size = " << productsQueue.size() << std::endl;
            if (productsQueue.size() == 1) {
                condNotEmpty.notifyAll();
            }
        }

        T take() {
            VitaLockGuard lockGuard(mutex);
            while (productsQueue.empty()) {
                std::cout << "empty-take wait  !" << std::endl;
                condNotEmpty.wait();
            }
            T t = productsQueue.front();
            productsQueue.pop();
            std::cout << "take size = " << productsQueue.size() << std::endl;
            if (productsQueue.size() == capacity - 1) {
                condNotFull.notifyAll();
            }
            return t;
        }

        int getSize() {
            VitaLockGuard lockGuard(mutex);
            return productsQueue.size();
        }

        int getCapacity() {
            VitaLockGuard lockGuard(mutex);
            return capacity;
        }

    private:
        VitaMutex mutex;
        VitaCondition condNotFull;
        VitaCondition condNotEmpty;
        std::queue<T> productsQueue;
        int capacity;
    };

} // Vita

#endif //MYNETLIB_VITABLOCKINGQUEUE_H

