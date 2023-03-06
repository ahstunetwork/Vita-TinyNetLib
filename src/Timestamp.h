//
// Created by vitanmc on 2023/3/5.
//

#ifndef VITANETLIB_TIMESTAMP_H
#define VITANETLIB_TIMESTAMP_H

#include <unistd.h>
#include <iostream>

namespace Vita {

    class Timestamp {
    public:
        explicit Timestamp();
        explicit Timestamp(int64_t mSeconds);
        static Timestamp now();
        std::string to_string();

    private:
        int64_t mSeconds;
    };


} // Vita

#endif //VITANETLIB_TIMESTAMP_H
