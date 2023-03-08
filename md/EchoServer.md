+ 创建EventLoop，该EventLoop为用户代码层创建并且唯一
+ 创建一个地址结构，InetAddr的构造函数有很多种，不过我们对服务端的ip兴趣不大（因为通常是固定的），但port时长需要指定
+ 创建一个EchoServer对象，该对象内部其实就是封装了TcpServer类以及需要发给下层的回调函数。TcpServer类为网络库的核心，关于TcpServer类的设计以及全流程分析见后续温度
+ TcpServer::start实际运行Acceptor的Listen，同时还有EventLoopThreadPool的构造
+ EventLoop::loop实际上是跑通程序的主事件循环池，Accetor运行在其中
```cpp
#include <string>

#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>


class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的subloop线程数量
        server_.setThreadNum(3);
    }
    void start()
    {
        server_.start();
    }

private:
    // 连接建立或断开的回调函数
    void onConnection(const TcpConnectionPtr &conn)   
    {
        if (conn->connected())
        {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        // conn->shutdown();   // 关闭写端 底层响应EPOLLHUP => 执行closeCallback_
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main() {
    EventLoop loop;
    InetAddress addr(8002);
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
    loop.loop();
    return 0;
}
```

