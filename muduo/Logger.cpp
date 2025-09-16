#include "Logger.hpp"

#include <iostream>
Logger& Logger::instance(){
    static Logger logger;
    return logger;
}
void Logger::setLogLevel(int level){
    logLevel_=level;
}
// 写日志 
void Logger::log(std::string msg){
    switch(logLevel_){
        case INFO:
            std::cout<<"[INFO]";
            break;
        case ERROR:
            std::cout<<"[ERROR]";
            break;
        case FATAL:
            std::cout<<"[FATAL]";
            break;
        case DEBUG:
            std::cout<<"[DEBUG]";
            break;
        default:
            break;
    }
    //打印时间和msg
    std::cout<<Timestamp::now().tostring()<<"  :  "<<msg<<std::endl;
}