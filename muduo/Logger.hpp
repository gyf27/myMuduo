#pragma once

#include <string>

#include "noncopyable.hpp"
#include "Timestamp.hpp"

#define LOG_INFO(logmsgFormat,...) \
    do{ \
        Logger &logger=Logger::instance(); \
        logger.setLogLevel(INFO); \
        char buf[1024]; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
    }while(0)

#define LOG_FATAL(logmsgFormat,...) \
    do{ \
        Logger &logger=Logger::instance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024]; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
        exit(-1); \
    }while(0)

#define LOG_ERROR(logmsgFormat,...) \
    do{ \
        Logger &logger=Logger::instance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024]; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
    }while(0)
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat,...) \
    do{ \
        Logger &logger=Logger::instance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024]; \
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \
        logger.log(buf); \
    }while(0)
#else
    #define LOG_DEBUG(logmsgFormat,...)
#endif

enum Loglevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG
};

class Logger: noncopyable{
public:
    //获取日志的唯一实例对象
    static Logger& instance();
    //
    void setLogLevel(int level);
    void log(std::string msg);
private:
    int logLevel_;
    Logger(){}
};