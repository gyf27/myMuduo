#pragma once
/*
    Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN等
    还绑定了poller返回的具体事件
*/
#include <sys/epoll.h>
#include <functional>
#include <memory>

#include "noncopyable.hpp"
#include "Timestamp.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"

class EventLoop;

class Channel:noncopyable{
public:
    using EventCallback= std::function<void()> ;
    using ReadEventCallback=std::function<void(Timestamp)>;
    Channel(EventLoop* loop,int fd);
    ~Channel();

    //fd得到poller通知以后，处理事件——调用相应的回调
    void handleEvent(Timestamp reveiveTime);
    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb){readCallback_=std::move(cb);}
    void setWriteCallback(EventCallback cb){writeCallback_=std::move(cb);}
    void setCloseCallback(EventCallback cb){closeCallback_=std::move(cb);}
    void setERRORCallback(EventCallback cb){errorCallback_=std::move(cb);}
    //防止Channel被手动remove掉，还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd()const {return fd_;}
    int events()const {return events_;}
    void set_revents(int revt){revents_=revt;}

    void enableReading(){events_|=kReadEvent; update();}
    void disableReading(){events_&=~kReadEvent; update();}
    void enableWriting(){events_|=kWriteEvent; update();}
    void disableWriting(){events_&=~kWriteEvent; update();}
    void disableAll(){events_=kNoneEvent; update();}

    //返回fd当前事件的状态
    bool isNoneEvent() const{return kNoneEvent==events_;}
    bool isWriting() const{return kWriteEvent&events_;}
    bool isReading() const{return kReadEvent&events_;}

    int index(){return index_;}
    void set_index(int idx){index_=idx;}

    EventLoop* ownerLoop(){return loop_;}
    void remove();
private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;  //事件循环
    const int fd_;  //poller监听的对象
    int events_;        //注册fd感兴趣的事件
    int revents_;        //返回具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    //因为Channel能获知fd最终发生事件的revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

