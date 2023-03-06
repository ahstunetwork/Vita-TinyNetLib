//
// Created by vitanmc on 2023/3/5.
//

#include "Logger.h"
#include <iostream>

#include "Timestamp.h"

namespace Vita {






// ��ȡ��־Ψһ��ʵ������ ����
    Logger &Logger::instance()
    {
        static Logger logger;
        return logger;
    }

// ������־����
    void Logger::setLogLevel(int level)
    {
        logLevel_ = level;
    }

// д��־ [������Ϣ] time : msg
    void Logger::log(std::string msg)
    {
        std::string pre = "";
        switch (logLevel_)
        {
            case INFO:
                pre = "[INFO]";
                break;
            case ERROR:
                pre = "[ERROR]";
                break;
            case FATAL:
                pre = "[FATAL]";
                break;
            case DEBUG:
                pre = "[DEBUG]";
                break;
            default:
                break;
        }

        // ��ӡʱ���msg
        std::cout << pre + Timestamp::now().to_string() << " : " << msg << std::endl;
    }



} // Vita