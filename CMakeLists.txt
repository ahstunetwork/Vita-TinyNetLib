cmake_minimum_required(VERSION 3.16)
project(VitaNetLib)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Wall -pthread")

add_executable(MyNetLib main.cpp mutex_tools/VitaMutex.cpp mutex_tools/VitaMutex.h mutex_tools/VitaCondition.cpp mutex_tools/VitaCondition.h mutex_tools/VitaCountDownLatch.cpp mutex_tools/VitaCountDownLatch.h mutex_tools/VitaBoundedBlockingQueue.cpp mutex_tools/VitaBoundedBlockingQueue.h src/Timestamp.cpp src/Timestamp.h src/Channel.cpp src/Channel.h src/NonCopyable.h src/EventLoop.cpp src/EventLoop.h src/Logger.cpp src/Logger.h src/Poller.cpp src/Poller.h src/EPollPoller.cpp src/EPollPoller.h src/Thread.cpp src/Thread.h src/EventLoopThread.cpp src/EventLoopThread.h src/EventLoopThreadPool.cpp src/EventLoopThreadPool.h src/InetAddress.cpp src/InetAddress.h src/Socket.cpp src/Socket.h src/Acceptor.cpp src/Acceptor.h src/TcpConnection.cpp src/TcpConnection.h src/Callbacks.h src/TcpServer.cpp src/TcpServer.h src/Buffer.cpp src/Buffer.h)
