//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_CALLBACKS_H
#define VITANETLIB_CALLBACKS_H


#include <memory>
#include <functional>

class Buffer;

class TcpConnection;

class Timestamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;

using MessageCallback = std::function<void(const TcpConnectionPtr &,
                                           Buffer *,
                                           Timestamp)>;


#endif //VITANETLIB_CALLBACKS_H
