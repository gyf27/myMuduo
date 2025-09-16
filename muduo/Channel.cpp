#include "Channel.hpp"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    :loop_(loop),
    fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    tied_(false)
    {}

Channel::~Channel(){}

void Channel::tie(const std::shared_ptr<void>& obj){
    tie_=obj;
    tied_=true;
}

//向poller更新事件
void Channel::update(){
    //通过Channel所属的loop调用poller的相应方法，进行事件注册

    //add code
    loop_->updateChannel(this);
}

//在Channel所属的loop中，把当前的Channel删除
void Channel::remove(){
    //add code
    loop_->removeChannel(this);
}

//fd得到poller通知以后，处理事件——调用相应的回调
//资源绑定判断，如果绑定过资源，则tied_为真，此时看能否通过.lock()方法将weak指针升级为shared指针，
//如果可以，则说明现在任然绑定着资源，需要handle；如果无法升级guard=nullptr，并不进行handle
void Channel::handleEvent(Timestamp receiveTime){
    std::shared_ptr<void> guard;
    if(tied_){
        guard=tie_.lock();
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }
    else{
        handleEventWithGuard(receiveTime);
    }
}

//根据poller通知的channel发生的具体事件，由channel负责具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime){
    //对端关闭且无数据可读
    LOG_INFO("channel handleEvent revents:%d",revents_);
    if((revents_&EPOLLHUP)&&!(revents_&EPOLLIN)){
        if(closeCallback_){
            closeCallback_();
        }
    }
    if(revents_&EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }
    if(revents_&(EPOLLIN|EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }
    if(revents_&EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}