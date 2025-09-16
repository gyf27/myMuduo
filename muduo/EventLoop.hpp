#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

#include "Timestamp.hpp"
#include "noncopyable.hpp"
#include "CurrentThread.hpp"

class Channel;
class Poller;

class EventLoop:noncopyable{
public:
	using Functor=std::function<void()>;

	EventLoop();
	~EventLoop();

	void loop();  //开启Loop
	void quit();	//退出Loop

	Timestamp pollReturnTime() const {return pollReturnTime_;}

	void runInLoop(Functor cb);  //在当前Loop执行cb
	void queueInLoop(Functor cb);	//把cb放入队列，唤醒loop所在的线程执行cb

	void wakeup();	//唤醒loop所在的线程

	//实际就是调用poller的方法
	void updateChannel(Channel* channel);
	void removeChannel(Channel* channel);
	bool hasChannel(Channel* channel);

	//判断loop是否在自己创建时的线程
	bool isInLoopThread()const {return threadId_==CurrentThread::tid();}
private:
	void handleRead();  //wakeup
	void doPendingFunctors();	//执行回调

	using ChannelList=std::vector<Channel*>;
	std::atomic_bool looping_;
	std::atomic_bool quit_;  //标识退出loop循环

	const pid_t threadId_;	//记录当前线程所在的线程id

	Timestamp pollReturnTime_;  //poller返回发生事件的channels的时间点
	std::unique_ptr<Poller> poller_;

	int wakeupFd_;  //主要作用：当mainloop获取一个新用户的channel，通过轮询选择subloop通过改成员唤醒subloop处理
	std::unique_ptr<Channel> wakeupChannel_;

	ChannelList activeChannels_;

	std::atomic_bool callingPendingFunctors_;  //标识当前loop是否有需要执行的回调
	std::vector<Functor> pendingFunctors_;  //存储loop需要执行的所有回调
	std::mutex mutex_;	//互斥锁，迎来保护上面的vector的线程安全
};