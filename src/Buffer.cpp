//
// Created by vitanmc on 2023/3/6.
//

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include "Buffer.h"

namespace Vita {


    /**
 * ��fd�϶�ȡ���� Poller������LTģʽ
 * Buffer���������д�С�ģ� ���Ǵ�fd�϶�ȡ���ݵ�ʱ�� ȴ��֪��tcp���ݵ����մ�С
 *
 * @description: ��socket�����������ķ�����ʹ��readv�ȶ���buffer_��
 * Buffer_�ռ������������뵽ջ��65536���ֽڴ�С�Ŀռ䣬Ȼ����append��
 * ��ʽ׷����buffer_���ȿ����˱���ϵͳ���ô����������ֲ�Ӱ�����ݵĽ��ա�
 **/
 



// readv �� writev ����������һ�κ��������ж���д�����������������
// ��ʱҲ��������������Ϊɢ������scatter read���;ۼ�д��gather write����

    ssize_t Buffer::readFd(int fd, int *saveErrno) {
        // ջ����ռ䣬���ڴ��׽���������ʱ
        // ��buffer_��ʱ������ʱ�ݴ����ݣ���buffer_���·����㹻�ռ���ڰ����ݽ�����buffer_��
        char extrabuf[65536] = {0}; // ջ���ڴ�ռ� 65536/1024 = 64KB

        /*
        struct iovec {
            ptr_t iov_base; // iov_baseָ��Ļ�������ŵ���readv�����յ����ݻ���writev��Ҫ���͵�����
            size_t iov_len; // iov_len�ڸ�������·ֱ�ȷ���˽��յ���󳤶��Լ�ʵ��д��ĳ���
        };
        */

        // ʹ��iovec�������������Ļ�����
        // ����Buffer�ײ㻺����ʣ��Ŀ�д�ռ��С ��һ������ȫ�洢��fd����������
        struct iovec vec[2];
        // �������readv�������д��������ǰ������
        const size_t writable = writableBytes();

        // ��һ�黺������ָ���д�ռ�
        vec[0].iov_base = begin() + writerIndex_;
        vec[0].iov_len = writable;
        // �ڶ��黺������ָ��ջ�ռ�
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof(extrabuf);

        // when there is enough space in this buffer, don't read into extrabuf.
        // when extrabuf is used, we read 128k-1 bytes at most.
        // ����֮����˵���128k-1�ֽڣ�����Ϊ��writableΪ64k-1����ô��Ҫ���������� ��һ��64k-1 �ڶ���64k ��������128k-1
        // �����һ��������>=64k �Ǿ�ֻ����һ�������� ����ʹ��ջ�ռ�extrabuf[65536]������
        const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
        const ssize_t n = ::readv(fd, vec, iovcnt);

        if (n < 0) {
            *saveErrno = errno;
        } else if (n <= writable) // Buffer�Ŀ�д�������Ѿ����洢��������������
        {
            writerIndex_ += n;
        } else // extrabuf����Ҳд����n-writable���ȵ�����
        {
            writerIndex_ = buffer_.size();
            append(extrabuf, n - writable); // ��buffer_���� ����extrabuf�洢����һ��������׷����buffer_
        }
        return n;
    }

// inputBuffer_.readFd��ʾ���Զ����ݶ���inputBuffer_�У��ƶ�writerIndex_ָ��
// outputBuffer_.writeFd��ʾ������д�뵽outputBuffer_��
// ��readerIndex_��ʼ������дreadableBytes()���ֽ�
    ssize_t Buffer::writeFd(int fd, int *saveErrno) {
        // д����û��ô�ི����ֱ��ȫд��ȥ������
        ssize_t n = ::write(fd, peek(), readableBytes());
        if (n < 0) {
            *saveErrno = errno;
        }
        return n;
    }


} // Vita