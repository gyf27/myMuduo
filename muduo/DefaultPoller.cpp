#include "Poller.hpp"
#include "EpollPoller.hpp"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop* loop){
    if(::getenv("MODUO_USE_POLL")){
        return nullptr;  //生成poll的实例
    }
    else{
        return new EpollPoller(loop);  //生成epoll的实例
    }
}