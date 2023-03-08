<a name="ckPTU"></a>
# 1 mutex
<a name="BEzpp"></a>
## 01 Mutex & lockGurad

1. mutex 设计方法：
   1. 封装基本的互斥量 pthread_mutex_t，需要向外提供如下
      1. `lock`：仅供 VitaLockGuard 使用
      2. `unlock`：仅供 VitaLockGuard 使用
      3. `get_raw_mutex`：仅供封装的 VitaConditon 调用，获取原始锁对象 
      4. `try_lock`：try 一下
   2. 构造和析构分别调用原始 mutex 的 init 和 destory
2. lockguard 设计：
   1. 持有封装的 &VitaMutex，构造时 VitaMutex::lock，析构时 VitaMutex:unlock
```cpp
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
        }
        ~VitaLockGuard() {
            vitaMutex.unlock();
        }
    private:
        VitaMutex vitaMutex;
    };

} //Vita
```
<a name="aIDHT"></a>
## 02 Conditon_variable

1. 设计：持有 &VitaMutex 和 pthread_cond_t，对外提供
   1. `wait`: 阻塞在锁上，用户可用，最好是被高级并发工具调用，下同
   2. `notify`：唤醒一个阻塞在此 VitaMutex 上的线程
   3. `notifyAll`：唤醒所有阻塞在此 VitaMutex 上的线程
```cpp
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
```


<a name="GedPd"></a>
## 03 CountDownLatch

1. 介绍：CountDownLatch 可译为倒计数，发起的命令是：ready,3,2,1,go！通常用作一批线程具有统一的条件或者统一等待的场合
   1. 发起号令：所有玩家做好了准备，时间一到，开始选择英雄
   2. 等待完成：游戏加载界面，所有玩家加载完毕，游戏才开始
2. 设计：
   1. CountDownLatch 和 BoundedBlockingQueue 一样，为独立自含的同步互斥工具，所以封装了 VitaMutex , VitaCondition 和一个计数器 counter
   2. 对外提供接口（以等待完成工作方式为例）：
      1. `countDown`：代表一个成员到达终点
      2. `wait`：代表需要等待
      3. `getCount`：返回还未完成任务的成员的数量
3. 代码
```cpp
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
```

4. 他山之石

![image.png](https://cdn.nlark.com/yuque/0/2023/png/26049599/1677294519543-56736d45-ad0f-4b24-b9a8-6c62ef8164a0.png#averageHue=%232b3034&clientId=uada4e3a9-4678-4&from=paste&id=u77cabb4b&name=image.png&originHeight=388&originWidth=590&originalType=url&ratio=2&rotation=0&showTitle=true&size=288513&status=done&style=none&taskId=u281660ac-1c6c-4122-8cb7-8b440583e1c&title=Java%20CountDownLatch%20%E4%BE%8B%E5%AD%90 "Java CountDownLatch 例子")

<a name="pCbs8"></a>
## 04 BoundedBlockingQueue

1. 设计：教科书式的设计
   1. 一个 VitaMutex，为 VitaCondition 所使用，作为内部互斥器
   2. 两个 VitaCondition，唤醒两个不同的阻塞原因
      1. queueNotFull：队满时推出一个，唤醒此
      2. queueNotEmpty：队空时加入一个，唤醒此
2. 对外接口：
   1. `put`：加入一个
   2. `take`：取出一个
   3. `getXXX`：获取队列信息，如 size，capacity 等
3. 代码
```cpp
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
                // std::cout << "full put wait !" << std::endl;
                condNotFull.wait();
            }
            productsQueue.push(val);
            // std::cout << "push size = " << productsQueue.size() << std::endl;
            if (productsQueue.size() == 1) {
                condNotEmpty.notifyAll();
            }
        }

        T take() {
            VitaLockGuard lockGuard(mutex);
            while (productsQueue.empty()) {
                // std::cout << "empty-take wait  !" << std::endl;
                condNotEmpty.wait();
            }
            T t = productsQueue.front();
            productsQueue.pop();
            // std::cout << "take size = " << productsQueue.size() << std::endl;
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
```
<a name="yy8fp"></a>
## 06 Producer-Consumer Problem

1. 任务流程：先开三个生产者线程和两个消费者线程，一秒钟后，再开三个消费者线程
2. 期望能够看到的画面
   1. 首先能看到阻塞队列的数据不断增大：produce > consumer，能看看到频繁的 "full put wait"
   2. 一秒钟后，新线程假如，阻塞队列数据不端减小：produce < consumer，能够看到频繁的 "empty take wait"
3. 程序代码
```cpp
#include <unistd.h>
#include <bits/stdc++.h>

#include "mutex_tools/VitaMutex.h"
#include "mutex_tools/VitaCondition.h"
#include "mutex_tools/VitaBoundedBlockingQueue.h"


Vita::VitaBoundedBlockingQueue<std::string> productQueue(10);

Vita::VitaMutex IOMutex;

void *produce(void *args) {
    srand(time(NULL));

    for (int i = 0;; i++) {
        std::string product = std::to_string(i);
        productQueue.put(product);
        int sleep_time = rand() % 2000;
        {
            Vita::VitaLockGuard lockGuard(IOMutex);
            std::cout << "<<< " << product << "    sleep time " << sleep_time << std::endl;
        }
        usleep(sleep_time);
    }
    return nullptr;
}

void *consumer(void *args) {
    srand(time(NULL));

    for (int i = 0;; i++) {
        std::string product = productQueue.take();
        int sleep_time = rand() % 2000;
        {
            Vita::VitaLockGuard lockGuard(IOMutex);
            std::cout << ">>> " << product << "    sleep time " << sleep_time << std::endl;
        }
        usleep(sleep_time);
    }
    return nullptr;
}


int main() {
    std::cout << "production & consumer problem" << std::endl;

    pthread_t produceTid[3];
    pthread_t consume[5];

    for (int i = 0; i < 3; i++) {
        pthread_create(&produceTid[i], NULL, produce, NULL);
    }

    for (int i = 0; i < 2; i++) {
        pthread_create(&consume[i], NULL, consumer, NULL);
    }

    sleep(1);

    std::cout << "===========add_new consumer thread========" << std::endl;
    for (int i = 2; i < 5; i++) {
        pthread_create(&consume[i], NULL, consumer, NULL);
    }

    for (int i = 0; i < 3; i++) {
        pthread_join(produceTid[i], NULL);
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(consume[i], NULL);
    }

    std::cout << "exit" << std::endl;
    return 0;
}
```

4. 程序调试

![image.png](https://cdn.nlark.com/yuque/0/2023/png/26049599/1677288559651-c1f6355a-d983-4c3c-b124-9dae9d60ad3f.png#averageHue=%23282d35&clientId=uf404f63a-4b45-4&from=paste&height=260&id=u8cd72271&name=image.png&originHeight=519&originWidth=1140&originalType=binary&ratio=2&rotation=0&showTitle=true&size=58303&status=done&style=none&taskId=u01c85429-02f4-48e4-982f-00bb114be15&title=%E7%A8%8B%E5%BA%8F%E7%AC%AC%E4%B8%80%E7%A7%92%E5%86%85%EF%BC%8C%E9%98%BB%E5%A1%9E%E9%98%9F%E5%88%97%E5%A1%AB%E6%BB%A1%EF%BC%8C%E5%81%B6%E5%B0%94%E5%87%BA%E7%8E%B0%E7%9A%84full%20put%20wait&width=570 "程序第一秒内，阻塞队列填满，偶尔出现的full put wait")

![image.png](https://cdn.nlark.com/yuque/0/2023/png/26049599/1677288463785-d63eb16a-c595-4b07-bbed-7018c0d94a0a.png#averageHue=%23282c34&clientId=uf404f63a-4b45-4&from=paste&height=321&id=u631c2efa&name=image.png&originHeight=641&originWidth=1683&originalType=binary&ratio=2&rotation=0&showTitle=true&size=73181&status=done&style=none&taskId=u14d52612-356c-4761-9e59-72e6a161f7c&title=%E4%B8%80%E7%A7%92%E9%92%9F%E6%97%B6%E5%80%99%EF%BC%8C%E5%8A%A0%E5%85%A5%E6%96%B0%E7%BA%BF%E7%A8%8B&width=841.5 "一秒钟时候，加入新线程")

![image.png](https://cdn.nlark.com/yuque/0/2023/png/26049599/1677288456035-4e8e439f-087b-4a91-8ee3-2a8dbd2585a2.png#averageHue=%23282d35&clientId=uf404f63a-4b45-4&from=paste&height=287&id=ub44ebaaf&name=image.png&originHeight=573&originWidth=1082&originalType=binary&ratio=2&rotation=0&showTitle=true&size=64738&status=done&style=none&taskId=uc119682a-dbaf-44cc-84a7-2443ffdbbf0&title=%E9%A2%91%E7%B9%81%E7%9A%84empty-take%20wait&width=541 "频繁的empty-take wait")
<a name="Uxisz"></a>
## 07 Summary

1. 其实这部分应该分类为：
   1. lockGuard 和 mutex 是一家子
   2. Condition 是对现成的 mutex 实现阻塞
   3. BlockQueue 为独立的并发工具，完全 self-contained
2. 使用高级并发工具 BlockingQueue，已经实现最高级的线程安全，用户使用无需额外加锁，完全可以当成单线程程序使用
