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

// �����ײ�Ļ��������Ͷ���
    class Buffer {
    public:
        // ��ճ����Ԥ��ͷ���ռ�
        static const size_t kCheapPrepend = 8;
        static const size_t kInitialSize = 1024;


        // buffer�Ĺ��캯����ֻ��Ҫ�����ʼ����������С
        // 1. kCheapPrepend = kCheapPrepend + initialSize
        // 2. readerIndex = kCheapPrepend
        // 3. writeIndex = kCheapPrepend
        // ����ʼ����ʱ�����д���±궼��kCheapPrepend
        explicit Buffer(size_t initialSize = kInitialSize)
                : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend) {
        }


        // ���ؿɶ�����д��Ԥ���ռ�Ĵ�С
        size_t readableBytes() const { return writerIndex_ - readerIndex_; }

        size_t writableBytes() const { return buffer_.size() - writerIndex_; }

        size_t prependableBytes() const { return readerIndex_; }

        // ���ػ������пɶ����ݵ���ʼ��ַ
        // ���ص��ǵ����������õ���vector���������д����
        const char *peek() const { return begin() + readerIndex_; }


        // ���ô˺�������һ�£������ζ�ȡ�����ݳ�����û�аѿɶ�����������
        // ����û���꣬�� len < readableBytes -- �޸�readerIndex�±�
        // ���Ƕ����� -- readerIndex, writerIndexȫ����λ���ص�kCheapPrepend
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

        // ��onMessage�����ϱ���Buffer���� ת��string���͵����ݷ���
        // ���纯���������ALL -- �ú�����ѿɶ������ڵ��������ݣ�����string����
        std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }

        std::string retrieveAsString(size_t len) {
            std::string result(peek(), len);
            retrieve(len); // ����һ��ѻ������пɶ��������Ѿ���ȡ���� ����϶�Ҫ�Ի��������и�λ����
            return result;
        }

        // buffer_.size - writerIndex_
        // �ú�����ɵ�Ŀ��Ҫȷ��ĳһ��д�����ɹ�����ҪensureWritableBytes�����㹻�ռ�
        // ����ɶ��ռ� writableBytes < len -- makeSpace
        void ensureWritableBytes(size_t len) {
            if (writableBytes() < len) {
                makeSpace(len); // ����
            }
        }
        void makeSpace(size_t len) {
            /**
             * | kCheapPrepend |xxx| reader | writer |                     // xxx��ʾreader���Ѷ��Ĳ���
             * | kCheapPrepend | reader ��          len          |
             **/


            // Ҳ����˵ len > xxx + writer�Ĳ���
            // �������������϶�������д�������ˣ���ô��ô����ռ��أ�
            // ����prependingռ���˺ܶ�ռ䣬��ô����ֱ����ǰŲ
            // ��prepending�ռ�Ҳ�޼����£�ֻ�ܿ����� -- ֱ��resize
            if (writableBytes() + prependableBytes() - kCheapPrepend < len)
            {
                buffer_.resize(writerIndex_ + len);
            } else // ����˵�� len <= xxx + writer ��reader�ᵽ��xxx��ʼ ʹ��xxx������һ�������ռ�
            {
                size_t readable = readableBytes(); // readable = reader�ĳ���
                // copy��������readerIndex��writeIndex��һ�������ݶ���kCheapPrepending
                std::copy(begin() + readerIndex_,
                          begin() + writerIndex_,
                          begin() + kCheapPrepend);
                readerIndex_ = kCheapPrepend;
                writerIndex_ = readerIndex_ + readable;
            }
        }

        // ��[data, data+len]�ڴ��ϵ�������ӵ�writable����������
        // append���õĵط�Ϊ����fd�϶����ݵ�ʱ��buffer�ռ�һʱ��᲻�����ȶ���extrabuf����
        // �Ȱѻ���������Ȼ�����buffer���ݣ�extrabuf����append��buffer����
        void append(const char *data, size_t len) {
            ensureWritableBytes(len);
            std::copy(data, data + len, beginWrite());
            writerIndex_ += len;
        }

        char *beginWrite() { return begin() + writerIndex_; }

        const char *beginWrite() const { return begin() + writerIndex_; }

        // ��fd�϶�ȡ����
        ssize_t readFd(int fd, int *saveErrno);

        // ͨ��fd��������
        ssize_t writeFd(int fd, int *saveErrno);

    private:
        // vector�ײ�������Ԫ�صĵ�ַ Ҳ�����������ʼ��ַ
//        char *begin() { return &*buffer_.begin(); }
        char *begin() { return buffer_.data(); }
        const char *begin() const { return buffer_.data(); }


        std::vector<char> buffer_;
        size_t readerIndex_;
        size_t writerIndex_;
    };


} // Vita

#endif //VITANETLIB_BUFFER_H
