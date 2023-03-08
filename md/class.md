<a name="mpwjK"></a>
# 01 EchoServer
+ 见 EchoServer 代码：[EchoServer解析](./md/EchoServer.md)

<a name="q0wor"></a>
# 02 全流程分析

<a name="LuFrH"></a>
## 01 main.cpp

<a name="LnAqe"></a>
## 02 EventLoop
<a name="dKviX"></a>
### 概述
程序一上来就开启一个 EventLoop，这是何为？EventLoop做的事情其实不算多，而且大多数工作都是为了他自己，比如 eventfd 的配套措施。EventLoop 构造结束，创建了一个全局的 MainLoop，这个MainLoop作为整个TcpServer类的主循环池
<a name="jrLqx"></a>
### EventLoop构造函数

1. EventLoop 构造函数功能解析
   1. 获取新的 poller
   2. 创建 eventfd
   3. 为 eventfd 绑定配套的 channel
   4. 为 eventfd 的channel 设置相应的读回调
   5. 将 eventfd 监听读事件挂载到epoll树上
2. 上述这些都是 EventLoop 的构造函数做的事情，EventLoop 比较省心的是不需要传入任何参数，意味着 EventLoopThread 开辟的 EventLoop 对象也是完整的行为。
```cpp
EventLoop::EventLoop()
        : looping_(false), 
			quit_(false), 
			callingPendingFunctors_(false), 
			threadId_(CurrentThread::tid()),
          	poller_(Poller::newDefaultPoller(this)),
			wakeupFd_(createEventfd()),
          	wakeupChannel_(new Channel(this, wakeupFd_)) {
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    if (t_loopInThisThread) {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    } else {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading(); // 每一个EventLoop都将监听wakeupChannel_的EPOLL读事件了
}
```

<a name="dvezd"></a>
### EventLoop::doPendingFunctors
函数的作用是执行高层回调，但是高层回调为何物？看后续解析
```cpp
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;    //标识当前eventloop正在执行高层回调

    // 交换的方式减少了锁的临界区范围 提升效率 同时避免了死锁
    // 如果执行functor()在临界区内 且functor()中调用queueInLoop()就会产生死锁
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor: functors) {
        functor(); // 执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}
```

<a name="rCzLY"></a>
### EventLoop::loop
为EventLoop类的核心，也是整个网络库的核心，EventLoop 持有 **ChannelList** 和一个 **poller**，在设置相应控制变量以后（looping，quit），便在while上循环处理事件。要做的任务有三：

1. **调用 poller::poll( ) -> EPollPoller::epoll_wait( )获取 activeChannel**
2. **循环处理 activeChannel，对于每个channel，调用channel上绑定的回调**
3. **处理高层回调 pendingFunctors**
```cpp
void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n", this);

    while (!quit_) {
        activeChannels_.clear();
        pollRetureTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel: activeChannels_) {
            // Poller监听哪些channel发生了事件 然后上报给EventLoop 通知channel处理相应的事件
            channel->handleEvent(pollRetureTime_);
        }
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping.\n", this);
    looping_ = false;
}
```
<a name="FGIBQ"></a>
### 
<a name="ritPV"></a>
### EventLoop::runInLoop
这个函数的意义是将高层回调塞到任务队列里，其中需要判断任务的来源。若任务来源（即那个线程调用的EventLoop::runInLoop）是EventLoop绑定的那个线程，说明是自己的任务，可以立即执行。若是其他线程交付过来的任务，需要queueInLoop，在任务队列里小小的排队。
```cpp
void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) // 当前EventLoop中执行回调
    {
        cb();
    } else // 在非当前EventLoop线程中执行cb，就需要唤醒EventLoop所在线程执行cb
    {
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //如果是高层发下来的回调，并且当先loop正在执行回调
    //则立即wakeup一次，立即唤醒新一轮的epoll_wait，从而执行此轮发下来的高层回调
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup(); // 唤醒loop所在线程
    }
}
```

<a name="G9XHa"></a>
### EventLoop::wakeup
wakeup机制是设计中的精华部分，因为EventLoop的事件循环里，做的不止是EventLoop本身的channel任务，还要执行高层的回调(doPendingFunctors)，由于这两类任务的不同，所以需要wakeup机制。<br />channel任务是依赖于epoll_wait返回的，如果只有channel任务，那么阻塞等待epoll_wait是可以的。但是由于高层回调可等不了那么长时间，所以要实现wakeup机制，以保证在高层回调发来的时候，可以立即（有机会）执行，而不是阻塞等待和高层毫不相关的epoll_wait返回。<br />wakeup机制实现的方法：实际上很简单，就是在该loop持有的poller里安排一个内鬼wakefd（以及配套的channel）。当高层有任务发下来的时候，此时 eventloop 阻塞在epoll_wait上，高层调用该eventloop->wakeup，想内鬼写入一点数据，epoll_wait 会立即触发，从而可以达到唤醒的目的。需要注意的是：wakeupfd是 eventloop 的固有成员变量，所以不管被谁调用，唤醒的都是 wakeupfd 所在的eventloop。
```cpp
void EventLoop::wakeup()
{
    uint64_t one = 1;
    //向内鬼写入一点数据，立即触发 epoll_wait
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}
```

<a name="UvTBm"></a>
## 03 TcpServer
<a name="mBBBZ"></a>
### 概述
从语义上理解，TcpServer 应该只有一个，TcpServer 内部可以开启很多附属的 Server，但是这个不归用户代码层管理。TcpServer 对象作为整个程序的大 Boss，会持有很多其他对象，基本的有：

1. 一个 EventLoop，即 main-loop 作为 TcpServer 的核心事件循环池
2. 一个 Acceptor，也是全局唯一的，作为 TcpServer 的负责处理连接信息的打工类
3. 一个 EventLoopThreadPool，作为 TcpServer 附属 reactor 的管理类
4. 一个 ConnectionList，掌握所有的 TcpConnection
<a name="K4JNf"></a>
### **TcpServer 构造函数**

1. 拿到 MainLoop 指针
2. 创建 Acceptor 对象（并且给他一个IP地址结构，作为 listenfd）
3. 创建 EventLoopThreadPool，拿到打工仔 reactor 的指针
4. 为 Acceptor 对象设置回调 HandleRead 处理读事件（新连接）-- **梦开始的地方**
```cpp
TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const std::string &nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    // 当有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生，执行handleRead()调用TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}
```

<a name="OYexA"></a>
## 04 Acceptor
<a name="QRFzz"></a>
### 概述
**Acceptor 专属于 （TcpServer对象）MainLoop**，并且唯一。只是负责处理新连接事件，想象传统服务器编程模式，一个唯一的 listenfd 只是赋值处理读事件，这里也是一样。Acceptor 的生命历程：

1. 在 TcpServer 的构造函数中被创建出来，绑定服务器端的 ip，该 ip 绑定一个 listenfd（socket 返回值），为该 listenfd 创建配套的 listenChannel
2. Accptor 对象在 TcpServer的构造函数体绑定上handleRead回调，在TcpServer::start中开始监听事件
3. listenfd 触发 epoll_wait，调用 Acceptor::handleRead 处理新连接，调用 ::accept4拿到**对端的地址结构**和**connfd**，据此再调用Acceptor::NewConnectionCallback，将新连接封装为 TcpConnection，该TcpConnection的基本信息有 acceptor 提供
   1. localAddr：本机（服务器）的 ip地址结构，accept 构造时直接持有
   2. peerAddr：对端的地址结构，有::accept4返回
   3. connfd：由 ::accept4 返回的fd，就地构造 VIta::Socket
   4. connChannel：就地构造
4. 在 Acceptor 调用的高层回调函数为TcpServer::NewConnection，这个函数隶属于 TcpServer类可不简单，TcpServer 类直接持有 EventLoopThreadPool，所以公平的选择一个 EventLoopThread，将新的TcpConnection任务交付。（为何TcpServer交付给Acceptor的函数，Acceptor可以直接使用并且调用TcpServer类的变量？这可能就是bind的时候要带着this指针的原因吧）
5. Acceptor 对象是main-loop中的独一份，他析构时整个TcpServer将不再处理新连接
<a name="Q2SzD"></a>
### Acceptor 构造函数

1. 拿到 MainLoop 指针，这是他打工的地方
2. 创建acceptSocket，与从 TcpServer 那里拿到的 ip 地址结构绑定，这是服务器 ip
3. 为 acceptSocket 创建配套的 channel
4. 为 acceptSocket 设置读事件回调（这个是库写死的，因为处理连接事件是唯一的），TcpServer 调用 Start方法的时候，开始listen，enableReading( )
<a name="sgK0d"></a>
### Acceptor::handleRead
Acceptor::handleRead为 listenfd 监听的读事件处理回调，TcpServer构造完成，调用TcpServer::start()之后，listenfd 就在 MainLoop 里监听读事件，监听到读事件到来，就会调用 Channel::handleEvent 处理，进而调用 Acceptor::handleRead。handRead 的基本任务：调用 ::accept4接收连接，将连接返回的 connfd 封装成 TcpConnection，再将TcpConnection负载均衡的发给son-Reactor（通过调用 NewConnectionCallback 回调完成，该回调在TcpServer中定义，并且在TcpServer的构造函数体内设置）
```cpp
void Acceptor::handleRead() {
    InetAddress peerAddr;  //开一个空地址结构，这个地址结构是对方的，即accept返回的
    int connfd = acceptSocket_.accept(&peerAddr);   //即调用 ::accept4(...)
    if (connfd >= 0) {
        if (NewConnectionCallback_) {
            NewConnectionCallback_(connfd, peerAddr); 
            // 轮询找到subLoop 唤醒并分发当前的新客户端的Channel
        } else {
            ::close(connfd);
        }
    } else { 
        // reprot_error 
    }
}
```

<a name="RCVFZ"></a>
### NewConnectionCallback
函数定义：`void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)`，可以看到，需要的参数是一个 sockfd，一个对端的地址结构（客户端地址结构，通过Acceptor::handleRead -> accept4接收连接时填充地址结构，同样sockfd是connfd）
```cpp
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
    // 轮询算法 选择一个subLoop 来管理 connfd 对应的 channel
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;  // 这里没有设置为原子类是因为其只在mainloop中执行 不涉及线程安全问题
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if (::getsockname(sockfd, (sockaddr *) &local, &addrlen) < 0) {
        LOG_ERROR("sockets::getLocalAddr");
    }

    InetAddress localAddr(local);


    // 新建一个TcpConnection（本函数还是咋acceptor里面完成）
    // TcpConnection参数
    // 1. ioLoop从EventLoopTHreadPool里轮询取出的
    // 2. sockfd从Acceptor直接bind的
    // 3. localAddr
    // 4. peerAddr
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer => TcpConnection的
    // 至于Channel绑定的则是TcpConnection设置的四个，handleRead,handleWrite...
    // 这下面的回调用于handlexxx函数中
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调
    conn->setCloseCallback(
            std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    ioLoop->runInLoop(
            std::bind(&TcpConnection::connectEstablished, conn));
}
```
<a name="Kx4E0"></a>
## 05 TcpConnection
<a name="OjpYr"></a>
### 概述
TcpConnection 为 Accepor 的兄弟类，Acceptor 为 TcpServer & MainLoop服务，TcpConnection 为 EventLoopThread & son_loop 服务。与Acceptor不同之处在于，Acceptor 只需要处理连接信息，即handleRead()，而TcpConnection 需要处理全事件，同样也在 TcpConnection 的构造函数里把该channel的所有事件绑定了，**TcpConnection 存储的数据包括：**

1. Tcp连接信息：包括服务端ip，对端ip，connfd，connfd 配套的 channel。
2. Tcp Channel上的信息：
   1. 库回调：handleXXX
   2. 高层回调：XXXCallback
3. Tcp 通信信息：inputBuffer，outputBuffer

<a name="ijZuV"></a>
### TcpConnection::handleRead
函数逻辑很简单，读自己 channel 里的数据，读到自己的 inputBuffer 里，读数据以后，再执行高层回调。从这里就能看出来库回调和高层回调的区别以及调用时机

1. 库回调：维持他身为服务器本该干的事，读 fd，写 fd，可以不跟外界打交道
2. 高层回调：一般是用户代码层指定，指定库回调读完数据以后，将读到的数据返回给TcpServer (我)，或者打印看看
```cpp
void TcpConnection::handleRead(Timestamp receiveTime) {
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) // 有数据到达
    {
        // 已建立连接的用户有可读事件发生了
        // 调用用户传入的回调操作onMessage
        // shared_from_this就是获取了TcpConnection的智能指针
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) // 客户端断开
    {
        handleClose();
    } else // 出错了
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
```
EchoServer::onMessage 函数定义，实际调用处为 TcpConnection::handleRead
```cpp
// 可读写事件回调
void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
{
    std::string msg = buf->retrieveAllAsString();
    conn->send(msg);
    // conn->shutdown();   // 关闭写端 底层响应EPOLLHUP => 执行closeCallback_
}
```

<a name="mE1fR"></a>
### TcpConnection::connectionEstablished
实际调用处是 TcpServer::newConnection，为新连接建立起 TcpConnection 的时候，从ThreadLoop 里选择一个 son-loop 加入，并且 <br />`ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));`<br />这行代码执行完毕，就是把新的 TcpConnection 挂载到了该 eventloop 的 epoll 树上
```cpp
// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 向poller注册channel的EPOLLIN读事件
    // 新连接建立 执行回调
    connectionCallback_(shared_from_this());
}
```

<a name="mfqIR"></a>
##  06 EventLoopThread
<a name="Lkku4"></a>
### 概述
顾名思义，EventLoopThread 类的作用是绑定一个 EventLoop（指针） 和一个 Thread（对象），Thread 和 EventLoop 都是现场构造出来，和 EventLoopThread类绑定的很死，无需外界传入他们（Thread 构造的参数--初始化函数需要传入）

<a name="Ebcco"></a>
### EventLoopThread::startLoop
该函数调用，EventLoopThread 封装的线程 Thread调用start，原始线程开始构造并且启动，发起的线程运行启动函数 EventLoopThread::ThreadFunc( )，该函数的作用是实际创建一个EventLoop对象，并且用条件变量保证创建的EventLoop对象能够被EventLoopThread 持有的 EvnetLoop 指针拿到。完成 EventLoop 和 Thread 的绑定，**为什么要创建EventLoop的动作在新发起的线程里面创建，而不是直接创建？直接创建EventLoop的归属线程是谁？**main线程，因为是TcpServer持有EventLoopThreadPool，EventLoopThreadPool发起一系列EventLoopThread，所以在新发起的线程内创建EventLoop对象，并且与之绑定。
```cpp
EventLoop *EventLoopThread::startLoop() {
    thread_.start(); // 启用底层线程Thread类对象thread_中通过start()创建的线程
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

// 下面这个方法 是在单独的新线程里运行的
void EventLoopThread::threadFunc() {
    // 创建一个独立的EventLoop对象 和上面的线程是一一对应的 级one loop per thread
    // 这个EventLoop是子Reactor的EventLoop
    // 与MainReactor完全不同
    EventLoop loop;
    if (callback_) {
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    // 执行EventLoop的loop() 开启了底层的Poller的poll()
    loop.loop();
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
```


<a name="y4KSb"></a>
## 07 EventLoopThreadPool
<a name="ssaHe"></a>
### 概述
该类为一个数据管理类，管理的是一系列EventLoopThread，虽然徒有Pool的虚名，但确实是有一系列线程 `std::vector<std::unique_ptr<EventLoopThread>> threads_;`使用独占指针持有，该threadPool类对于这个 vector 的管理也很简单，只需要在构造的时候批量发起，在用的时候可以轮询使用即可。

<a name="kBmMI"></a>
### EventLoopThreadPool::start
ThreadInitCallback( ) 是给EventLoopThread构造EventLoop的时候调用的，回调函数参数为那个新的 EventLoop。在start函数内部，循环发起numThread数量个EventLoopThread，给他们的独占指针推导threadPool类的vector里面，然后 startLoop

那么问题来了？EventLoopThreadPool的回调函数是谁传进来的呢？ TcpServer !
```cpp
// using ThreadInitCallback = std::function<void(EventLoop *)>;
void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        // 底层创建线程 绑定一个新的EventLoop 并返回该loop的地址
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        //t->startLoop的功能，开启一个线程，并且返回一个pool
        loops_.push_back(t->startLoop());
    }

    // 整个服务端只有一个线程运行baseLoop
    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}
```
<a name="KaFd0"></a>
### TcpServer::start
TcpServer 的start做的事情非常多，首先就是让ThreadPool 开始 start，就是名为threadInitCallback_ 的TcpServer成员变量。然后TcpServer让持有的`BaseLoop->runInLoop`，运行的回调任务是 Acceptor::listen
```cpp
void TcpServer::start() {
    if (started_++ == 0)    // 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_);    // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}
```
