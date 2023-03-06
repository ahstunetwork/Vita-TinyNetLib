//
// Created by vitanmc on 2023/3/6.
//

#ifndef VITANETLIB_BUFFER_H
#define VITANETLIB_BUFFER_H

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

namespace Vita {

// 网络库底层的缓冲区类型定义
    class Buffer {
    public:
        // 防粘包的预留头部空间
        static const size_t kCheapPrepend = 8;
        static const size_t kInitialSize = 1024;


        // buffer的构造函数，只需要传入初始化数据区大小
        // 1. kCheapPrepend = kCheapPrepend + initialSize
        // 2. readerIndex = kCheapPrepend
        // 3. writeIndex = kCheapPrepend
        // 即初始化的时候读和写的下标都在kCheapPrepend
        explicit Buffer(size_t initialSize = kInitialSize)
                : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend) {
        }


        // 返回可读、可写、预留空间的大小
        size_t readableBytes() const { return writerIndex_ - readerIndex_; }

        size_t writableBytes() const { return buffer_.size() - writerIndex_; }

        size_t prependableBytes() const { return readerIndex_; }

        // 返回缓冲区中可读数据的起始地址
        // 返回的是迭代器，利用的是vector的是随机读写特征
        const char *peek() const { return begin() + readerIndex_; }


        // 调用此函数检索一下，看当次读取的数据长度有没有把可读数据区读完
        // 若是没读完，即 len < readableBytes -- 修改readerIndex下标
        // 若是读完了 -- readerIndex, writerIndex全部复位，回到kCheapPrepend
        void retrieve(size_t len) {
            if (len < readableBytes()) {
                readerIndex_ += len;
            } else // len == readableBytes()
            {
                retrieveAll();
            }
        }

        void retrieveAll() {
            readerIndex_ = kCheapPrepend;
            writerIndex_ = kCheapPrepend;
        }

        // 把onMessage函数上报的Buffer数据 转成string类型的数据返回
        // 正如函数名字里的ALL -- 该函数会把可读区域内的所有数据，读成string返回
        std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }

        std::string retrieveAsString(size_t len) {
            std::string result(peek(), len);
            retrieve(len); // 上面一句把缓冲区中可读的数据已经读取出来 这里肯定要对缓冲区进行复位操作
            return result;
        }

        // buffer_.size - writerIndex_
        // 该函数完成的目标要确保某一次写操作成功，需要ensureWritableBytes开辟足够空间
        // 如果可读空间 writableBytes < len -- makeSpace
        void ensureWritableBytes(size_t len) {
            if (writableBytes() < len) {
                makeSpace(len); // 扩容
            }
        }
        void makeSpace(size_t len) {
            /**
             * | kCheapPrepend |xxx| reader | writer |                     // xxx标示reader中已读的部分
             * | kCheapPrepend | reader ｜          len          |
             **/


            // 也就是说 len > xxx + writer的部分
            // 进到这个函数里，肯定是由于写区不够了，那么怎么扩大空间呢？
            // 若被prepending占据了很多空间，那么可以直接往前挪
            // 若prepending空间也无济于事，只能开大招 -- 直接resize
            if (writableBytes() + prependableBytes() - kCheapPrepend < len)
            {
                buffer_.resize(writerIndex_ + len);
            } else // 这里说明 len <= xxx + writer 把reader搬到从xxx开始 使得xxx后面是一段连续空间
            {
                size_t readable = readableBytes(); // readable = reader的长度
                // copy函数，把readerIndex到writeIndex这一部分数据读到kCheapPrepending
                std::copy(begin() + readerIndex_,
                          begin() + writerIndex_,
                          begin() + kCheapPrepend);
                readerIndex_ = kCheapPrepend;
                writerIndex_ = readerIndex_ + readable;
            }
        }

        // 把[data, data+len]内存上的数据添加到writable缓冲区当中
        // append调用的地方为：从fd上读数据的时候，buffer空间一时半会不够，先读到extrabuf里面
        // 先把活揽下来，然后后面buffer扩容，extrabuf数据append到buffer里面
        void append(const char *data, size_t len) {
            ensureWritableBytes(len);
            std::copy(data, data + len, beginWrite());
            writerIndex_ += len;
        }

        char *beginWrite() { return begin() + writerIndex_; }

        const char *beginWrite() const { return begin() + writerIndex_; }

        // 从fd上读取数据
        ssize_t readFd(int fd, int *saveErrno);

        // 通过fd发送数据
        ssize_t writeFd(int fd, int *saveErrno);

    private:
        // vector底层数组首元素的地址 也就是数组的起始地址
//        char *begin() { return &*buffer_.begin(); }
        char *begin() { return buffer_.data(); }
        const char *begin() const { return buffer_.data(); }


        std::vector<char> buffer_;
        size_t readerIndex_;
        size_t writerIndex_;
    };


} // Vita

#endif //VITANETLIB_BUFFER_H
