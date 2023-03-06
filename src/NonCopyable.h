//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_NONCOPYABLE_H
#define VITANETLIB_NONCOPYABLE_H

class NonCopyable {
public:
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
};

#endif //VITANETLIB_NONCOPYABLE_H
