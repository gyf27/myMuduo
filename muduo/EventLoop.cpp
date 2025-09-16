#include "EventLoop.hpp"
#include "Logger.hpp"
#include "Channel.hpp"
#include "Poller.hpp"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

__thread EventLoop* t_loopInThisThread=nullptr;  //防止一个线程创建多个loop

const int kPollTimeMs=10000;  //定义默认的Poller IO复用接口的超时时间

//创建wakeupfd用来notify subloop处理新来的channel
int createEventfd(){
	int evtfd=::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
	if(evtfd<0){
		LOG_FATAL("eventfd error :%d\n",errno);
	}
	return evtfd;
}

EventLoop::EventLoop()
	:looping_(false)
	,quit_(false)
	,threadId_(CurrentThread::tid())
	,poller_(Poller::newDefaultPoller(this))
	,wakeupFd_(createEventfd())
	,wakeupChannel_(new Channel(this,wakeupFd_))
	,callingPendingFunctors_(false)
{
	LOG_DEBUG("eventloop created  %p in thread %d\n",this,threadId_);
	if(t_loopInThisThread){
		LOG_FATAL("Another EventLoop %p exists in this thread %d \n",t_loopInThisThread,threadId_);
	}
	else{
		t_loopInThisThread=this;
	}

	//设置wakeupfd的事件类型以及发生事件后的回调操作
	wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
	wakeupChannel_->enableReading();
}

EventLoop::~EventLoop(){
	wakeupChannel_->disableAll();
	wakeupChannel_->remove();
	::close(wakeupFd_);
	t_loopInThisThread=nullptr;
}

void EventLoop::handleRead(){
	uint64_t one=1;
	ssize_t n=read(wakeupFd_,&one,sizeof(one));
	if(n!=sizeof(one)){
		LOG_ERROR("EventLoop::handleRead() reads %zd bytes instead of 8\n",n);
	}
}

void EventLoop::loop(){
	looping_=true;
	quit_=false;
	LOG_INFO("EventLoop %p start loopingi\n",this);
	while(!quit_){
		activeChannels_.clear();
		pollReturnTime_=poller_->poll(kPollTimeMs,&activeChannels_);
		for(Channel* channel:activeChannels_){
			//Poller监听哪些channel发生了事件，然后上报给EventLoop，然后通知channel执行相应的事件
			channel->handleEvent(pollReturnTime_);
		}
		//执行当前EventLoop事件循环需要处理的回调操作
		doPendingFunctors();
	}
	LOG_INFO("EventLoop %p stop looping \n",this);
	looping_=false;
}

void EventLoop::quit(){
	quit_=true;
	if(!isInLoopThread()){
		wakeup();
	}
}

//在当前Loop执行cb
void EventLoop::runInLoop(Functor cb){
	if(isInLoopThread()){ //在当前Loop线程执行cb
		cb();
	}
	else{	//非在当前Loop线程执行cb
		queueInLoop(cb);
	}
}

//把cb放入队列，唤醒loop所在的线程执行cb
void EventLoop::queueInLoop(Functor cb){
	{
		std::unique_lock<std::mutex> lock(mutex_);
		pendingFunctors_.emplace_back(cb);
	}
	//唤醒相应的，需要执行上面回调操作的loop的线程
	if(!isInLoopThread()||callingPendingFunctors_){
		wakeup();
	}
}

//唤醒loop所在的线程 向wakefd写一个数据
void EventLoop::wakeup(){
	uint64_t one=1;
	ssize_t n=write(wakeupFd_,&one,sizeof(one));
	if(n!=sizeof(one)){
		LOG_ERROR("EventLoop write %lu bytes instead of 8\n",n);
	}
}

//实际就是调用poller的方法
void EventLoop::updateChannel(Channel* channel){
	poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel){
	poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel){
	return poller_->hasChannel(channel);
}

//执行回调
void EventLoop::doPendingFunctors(){
	std::vector<Functor> functors;
	callingPendingFunctors_=true;
	{
		std::unique_lock<std::mutex> lock(mutex_);
		functors.swap(pendingFunctors_);
	}
	for(const Functor &functor:functors){
		functor();  //执行当前loop需要执行的回调操作
	}
	callingPendingFunctors_=false;
}